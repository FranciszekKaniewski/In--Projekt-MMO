#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define SECURITY_WIN32
#define SCHANNEL_USE_BLACKLISTS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winternl.h>
#include <security.h>
#include <schannel.h>
#include <wincrypt.h>

#include "TlsConnection.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <filesystem>
#include <iterator>
#include <mutex>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
using Deadline = Clock::time_point;

std::wstring wide(const std::string& text) {
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                          text.data(), static_cast<int>(text.size()), nullptr, 0);
    if(length <= 0) throw NetworkError("connection_failed");
    std::wstring result(length, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                        static_cast<int>(text.size()), result.data(), length);
    return result;
}

struct CertificateResources {
    HCERTSTORE roots = nullptr;
    HCERTCHAINENGINE engine = nullptr;
    PCCERT_CONTEXT remote = nullptr;
    PCCERT_CHAIN_CONTEXT chain = nullptr;
    ~CertificateResources() {
        if(chain) CertFreeCertificateChain(chain);
        if(remote) CertFreeCertificateContext(remote);
        if(engine) CertFreeCertificateChainEngine(engine);
        if(roots) CertCloseStore(roots, 0);
    }
};
}

struct TlsConnection::Impl {
    std::function<bool()> cancelled;
    std::mutex socketMutex;
    SOCKET socket = INVALID_SOCKET;
    bool winsock = false;
    CredHandle credentials{};
    CtxtHandle context{};
    SecPkgContext_StreamSizes sizes{};
    std::wstring hostname;
    std::string caFile;
    std::vector<char> encrypted, plaintext;

    explicit Impl(std::function<bool()> stop) : cancelled(std::move(stop)) {
        SecInvalidateHandle(&credentials);
        SecInvalidateHandle(&context);
    }

    ~Impl() {
        close();
        if(winsock) WSACleanup();
    }

    void closeSocket() {
        std::lock_guard<std::mutex> lock(socketMutex);
        if(socket != INVALID_SOCKET) { closesocket(socket); socket = INVALID_SOCKET; }
    }

    void close() {
        closeSocket();
        if(SecIsValidHandle(&context)) { DeleteSecurityContext(&context); SecInvalidateHandle(&context); }
        if(SecIsValidHandle(&credentials)) { FreeCredentialsHandle(&credentials); SecInvalidateHandle(&credentials); }
        encrypted.clear(); plaintext.clear();
    }

    bool wait(bool writing, Deadline deadline) {
        while(Clock::now() < deadline) {
            if(cancelled()) throw NetworkError("cancelled");
            fd_set ready;
            FD_ZERO(&ready); FD_SET(socket, &ready);
            fd_set failed = ready;
            const auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(deadline - Clock::now()).count();
            timeval delay{0, static_cast<long>(std::min<long long>(50000, std::max<long long>(1, remaining)))};
            const int result = select(0, writing ? nullptr : &ready, writing ? &ready : nullptr, &failed, &delay);
            if(result > 0) return true;
            if(result == SOCKET_ERROR) throw NetworkError("connection_closed");
        }
        return false;
    }

    void sendRaw(const char* bytes, std::size_t length, Deadline deadline) {
        while(length) {
            if(!wait(true, deadline)) throw NetworkError("connection_timeout");
            const int sent = ::send(socket, bytes, static_cast<int>(length), 0);
            if(sent == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK) continue;
            if(sent <= 0) throw NetworkError("connection_closed");
            bytes += sent; length -= sent;
        }
    }

    bool receiveRaw(Deadline deadline) {
        std::array<char, 16384> buffer;
        while(wait(false, deadline)) {
            const int count = recv(socket, buffer.data(), static_cast<int>(buffer.size()), 0);
            if(count == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK) continue;
            if(count <= 0) throw NetworkError("connection_closed");
            if(encrypted.size() + count > 65536) throw NetworkError("invalid_response");
            encrypted.insert(encrypted.end(), buffer.data(), buffer.data() + count);
            return true;
        }
        return false;
    }

    void keepExtra(const SecBuffer* buffers, std::size_t count) {
        std::size_t extra = 0;
        for(std::size_t i = 0; i < count; ++i)
            if(buffers[i].BufferType == SECBUFFER_EXTRA) extra = buffers[i].cbBuffer;
        if(extra > encrypted.size()) throw NetworkError("invalid_response");
        encrypted.erase(encrypted.begin(), encrypted.end() - static_cast<std::ptrdiff_t>(extra));
    }

    void handshake(Deadline deadline) {
        constexpr ULONG flags = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT |
            ISC_REQ_CONFIDENTIALITY | ISC_REQ_EXTENDED_ERROR | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM;
        bool first = !SecIsValidHandle(&context);
        for(;;) {
            if(cancelled()) throw NetworkError("cancelled");
            if(!first && encrypted.empty() && !receiveRaw(deadline)) throw NetworkError("connection_timeout");
            SecBuffer input[2]{{static_cast<ULONG>(encrypted.size()), SECBUFFER_TOKEN, encrypted.data()},
                               {0, SECBUFFER_EMPTY, nullptr}};
            SecBufferDesc in{SECBUFFER_VERSION, 2, input};
            SecBuffer token{0, SECBUFFER_TOKEN, nullptr};
            SecBufferDesc out{SECBUFFER_VERSION, 1, &token};
            ULONG attributes = 0;
            const SECURITY_STATUS status = InitializeSecurityContextW(&credentials,
                first ? nullptr : &context, hostname.data(), flags, 0, SECURITY_NATIVE_DREP,
                first ? nullptr : &in, 0, &context, &out, &attributes, nullptr);
            if(token.pvBuffer) {
                try {
                    if(token.cbBuffer) sendRaw(static_cast<char*>(token.pvBuffer), token.cbBuffer, deadline);
                } catch(...) { FreeContextBuffer(token.pvBuffer); throw; }
                FreeContextBuffer(token.pvBuffer);
            }
            if(status == SEC_E_INCOMPLETE_MESSAGE) {
                if(!receiveRaw(deadline)) throw NetworkError("connection_timeout");
                continue;
            }
            if(status != SEC_E_OK && status != SEC_I_CONTINUE_NEEDED)
                throw NetworkError("tls_failed");
            first = false;
            keepExtra(input, 2);
            if(status == SEC_E_OK) break;
        }
        if(QueryContextAttributesW(&context, SECPKG_ATTR_STREAM_SIZES, &sizes) != SEC_E_OK)
            throw NetworkError("tls_failed");
    }

    void verifyCertificate() {
        CertificateResources certs;
        std::ifstream file(std::filesystem::u8path(caFile), std::ios::binary);
        if(!file) throw NetworkError("certificate_missing");
        const std::string pem{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        certs.roots = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, CERT_STORE_CREATE_NEW_FLAG, nullptr);
        if(!certs.roots) throw NetworkError("tls_certificate");
        std::size_t offset = 0, added = 0;
        while((offset = pem.find("-----BEGIN CERTIFICATE-----", offset)) != std::string::npos) {
            const auto end = pem.find("-----END CERTIFICATE-----", offset);
            if(end == std::string::npos) throw NetworkError("tls_certificate");
            const std::string block = pem.substr(offset, end + 25 - offset);
            DWORD length = 0;
            if(!CryptStringToBinaryA(block.c_str(), static_cast<DWORD>(block.size()),
                CRYPT_STRING_BASE64HEADER, nullptr, &length, nullptr, nullptr)) throw NetworkError("tls_certificate");
            std::vector<BYTE> der(length);
            if(!CryptStringToBinaryA(block.c_str(), static_cast<DWORD>(block.size()),
                CRYPT_STRING_BASE64HEADER, der.data(), &length, nullptr, nullptr) ||
               !CertAddEncodedCertificateToStore(certs.roots, X509_ASN_ENCODING, der.data(), length,
                    CERT_STORE_ADD_USE_EXISTING, nullptr)) throw NetworkError("tls_certificate");
            ++added; offset = end + 25;
        }
        if(!added) throw NetworkError("tls_certificate");
        CERT_CHAIN_ENGINE_CONFIG engineConfig{};
        engineConfig.cbSize = sizeof(engineConfig);
        engineConfig.hExclusiveRoot = certs.roots;
        if(!CertCreateCertificateChainEngine(&engineConfig, &certs.engine) ||
           QueryContextAttributesW(&context, SECPKG_ATTR_REMOTE_CERT_CONTEXT, &certs.remote) != SEC_E_OK)
            throw NetworkError("tls_certificate");
        CERT_CHAIN_PARA chainParameters{};
        chainParameters.cbSize = sizeof(chainParameters);
        if(!CertGetCertificateChain(certs.engine, certs.remote, nullptr, certs.remote->hCertStore,
             &chainParameters, CERT_CHAIN_CACHE_ONLY_URL_RETRIEVAL, nullptr, &certs.chain))
            throw NetworkError("tls_certificate");
        SSL_EXTRA_CERT_CHAIN_POLICY_PARA sslPolicy{};
        sslPolicy.cbSize = sizeof(sslPolicy);
        sslPolicy.dwAuthType = AUTHTYPE_SERVER;
        sslPolicy.pwszServerName = hostname.data();
        CERT_CHAIN_POLICY_PARA policy{};
        policy.cbSize = sizeof(policy); policy.pvExtraPolicyPara = &sslPolicy;
        CERT_CHAIN_POLICY_STATUS status{};
        status.cbSize = sizeof(status);
        if(!CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_SSL, certs.chain, &policy, &status) ||
           status.dwError || certs.chain->TrustStatus.dwErrorStatus)
            throw NetworkError("tls_certificate");
    }

    void connect(const ServerConfig& config) {
        close();
        if(!winsock) {
            WSADATA data{};
            if(WSAStartup(MAKEWORD(2, 2), &data)) throw NetworkError("connection_failed");
            winsock = true;
        }
        hostname = wide(config.host); caFile = config.caFile;
        std::ifstream ca(std::filesystem::u8path(caFile));
        if(!ca) throw NetworkError("certificate_missing");
        const auto deadline = Clock::now() + std::chrono::milliseconds(config.timeoutMs);
        addrinfo hints{}; hints.ai_socktype = SOCK_STREAM; hints.ai_family = AF_UNSPEC;
        addrinfo* addresses = nullptr;
        const std::string port = std::to_string(config.port);
        if(getaddrinfo(config.host.c_str(), port.c_str(), &hints, &addresses))
            throw NetworkError("connection_failed");
        const std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> ownedAddresses(addresses, freeaddrinfo);
        bool connected = false;
        for(auto* address = addresses; address && !connected; address = address->ai_next) {
            if(cancelled()) throw NetworkError("cancelled");
            {
                std::lock_guard<std::mutex> lock(socketMutex);
                socket = ::socket(address->ai_family, address->ai_socktype, address->ai_protocol);
            }
            if(socket == INVALID_SOCKET) continue;
            u_long nonblocking = 1;
            if(ioctlsocket(socket, FIONBIO, &nonblocking)) { closeSocket(); continue; }
            const int result = ::connect(socket, address->ai_addr, static_cast<int>(address->ai_addrlen));
            if(result == 0) connected = true;
            else if(WSAGetLastError() == WSAEWOULDBLOCK && wait(true, deadline)) {
                int error = 0, length = sizeof(error);
                connected = getsockopt(socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &length) == 0 && !error;
            }
            if(!connected) closeSocket();
        }
        if(!connected) throw NetworkError("connection_failed");
        const BOOL noDelay = TRUE;
        setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));
        TLS_PARAMETERS parameters{};
        parameters.grbitDisabledProtocols = SP_PROT_SSL2 | SP_PROT_SSL3 | SP_PROT_TLS1_0 | SP_PROT_TLS1_1;
        SCH_CREDENTIALS settings{};
        settings.dwVersion = SCH_CREDENTIALS_VERSION;
        settings.dwFlags = SCH_CRED_MANUAL_CRED_VALIDATION | SCH_CRED_NO_DEFAULT_CREDS | SCH_USE_STRONG_CRYPTO;
        settings.cTlsParameters = 1; settings.pTlsParameters = &parameters;
        if(AcquireCredentialsHandleW(nullptr, const_cast<wchar_t*>(UNISP_NAME_W), SECPKG_CRED_OUTBOUND,
              nullptr, &settings, nullptr, nullptr, &credentials, nullptr) != SEC_E_OK)
            throw NetworkError("tls_failed");
        handshake(deadline);
        verifyCertificate();
    }

    void sendFrame(const std::string& body, std::chrono::milliseconds timeout) {
        if(body.size() > 4096) throw NetworkError("invalid_request");
        const auto deadline = Clock::now() + timeout;
        const auto length = static_cast<std::uint32_t>(body.size());
        std::string frame{static_cast<char>(length >> 24), static_cast<char>(length >> 16),
                          static_cast<char>(length >> 8), static_cast<char>(length)};
        frame += body;
        for(std::size_t offset = 0; offset < frame.size();) {
            const auto count = std::min<std::size_t>(sizes.cbMaximumMessage, frame.size() - offset);
            if(!count) throw NetworkError("tls_failed");
            std::vector<char> record(sizes.cbHeader + count + sizes.cbTrailer);
            std::copy_n(frame.data() + offset, count, record.data() + sizes.cbHeader);
            SecBuffer buffers[4]{{sizes.cbHeader, SECBUFFER_STREAM_HEADER, record.data()},
                {static_cast<ULONG>(count), SECBUFFER_DATA, record.data() + sizes.cbHeader},
                {sizes.cbTrailer, SECBUFFER_STREAM_TRAILER, record.data() + sizes.cbHeader + count},
                {0, SECBUFFER_EMPTY, nullptr}};
            SecBufferDesc message{SECBUFFER_VERSION, 4, buffers};
            if(EncryptMessage(&context, 0, &message, 0) != SEC_E_OK) throw NetworkError("tls_failed");
            for(int i = 0; i < 3; ++i)
                sendRaw(static_cast<char*>(buffers[i].pvBuffer), buffers[i].cbBuffer, deadline);
            offset += count;
        }
        SecureZeroMemory(frame.data(), frame.size());
    }

    std::optional<std::string> receiveFrame(std::chrono::milliseconds timeout) {
        const auto deadline = Clock::now() + timeout;
        for(;;) {
            if(plaintext.size() >= 4) {
                std::uint32_t length = 0;
                for(int i = 0; i < 4; ++i) length = (length << 8) | static_cast<unsigned char>(plaintext[i]);
                if(length > 4096) throw NetworkError("invalid_response");
                if(plaintext.size() >= length + 4) {
                    std::string result(plaintext.data() + 4, length);
                    plaintext.erase(plaintext.begin(), plaintext.begin() + length + 4);
                    return result;
                }
            }
            if(encrypted.empty() && !receiveRaw(deadline)) return std::nullopt;
            SecBuffer buffers[4]{{static_cast<ULONG>(encrypted.size()), SECBUFFER_DATA, encrypted.data()},
                {0, SECBUFFER_EMPTY, nullptr}, {0, SECBUFFER_EMPTY, nullptr}, {0, SECBUFFER_EMPTY, nullptr}};
            SecBufferDesc message{SECBUFFER_VERSION, 4, buffers};
            const auto status = DecryptMessage(&context, &message, 0, nullptr);
            if(status == SEC_E_INCOMPLETE_MESSAGE) {
                if(!receiveRaw(deadline)) return std::nullopt;
                continue;
            }
            if(status == SEC_I_RENEGOTIATE) {
                keepExtra(buffers, 4);
                handshake(deadline);
                verifyCertificate();
                continue;
            }
            if(status == SEC_I_CONTEXT_EXPIRED) throw NetworkError("connection_closed");
            if(status != SEC_E_OK) throw NetworkError("tls_failed");
            for(const auto& buffer : buffers) if(buffer.BufferType == SECBUFFER_DATA) {
                const auto* begin = static_cast<char*>(buffer.pvBuffer);
                plaintext.insert(plaintext.end(), begin, begin + buffer.cbBuffer);
            }
            if(plaintext.size() > 65536) throw NetworkError("invalid_response");
            keepExtra(buffers, 4);
        }
    }
};

TlsConnection::TlsConnection(std::function<bool()> cancelled) : impl(std::make_unique<Impl>(std::move(cancelled))) {}
TlsConnection::~TlsConnection() = default;
void TlsConnection::connect(const ServerConfig& config) { impl->connect(config); }
void TlsConnection::sendFrame(const std::string& body, std::chrono::milliseconds timeout) { impl->sendFrame(body, timeout); }
std::optional<std::string> TlsConnection::receiveFrame(std::chrono::milliseconds timeout) { return impl->receiveFrame(timeout); }
void TlsConnection::close() { impl->close(); }
void TlsConnection::interrupt() {
    std::lock_guard<std::mutex> lock(impl->socketMutex);
    if(impl->socket != INVALID_SOCKET) shutdown(impl->socket, SD_BOTH);
}

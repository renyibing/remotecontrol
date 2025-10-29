#include "rtc_ssl_verifier.h"

// WebRTC
#include <rtc_base/boringssl_certificate.h>

// OpenSSL
#include <openssl/x509.h>

#include "ssl_verifier.h"

RTCSSLVerifier::RTCSSLVerifier(bool insecure) : insecure_(insecure) {}

bool RTCSSLVerifier::Verify(const webrtc::SSLCertificate& certificate) {
  // If insecure, do not check the certificate
  if (insecure_) {
    return true;
  }

  // WebRTC m138 standard API: get the X509 certificate from cert_buffer()
  const auto& boring_cert =
      static_cast<const webrtc::BoringSSLCertificate&>(certificate);
  CRYPTO_BUFFER* cert_buffer = boring_cert.cert_buffer();

  // Convert from CRYPTO_BUFFER to X509
  const uint8_t* data = CRYPTO_BUFFER_data(cert_buffer);
  size_t len = CRYPTO_BUFFER_len(cert_buffer);
  X509* x509 = d2i_X509(nullptr, &data, len);

  if (!x509) {
    return false;
  }

  // Verify only a single certificate (chain information is not included in cert_buffer)
  bool result = SSLVerifier::VerifyX509(x509, nullptr);

  // Free memory
  X509_free(x509);

  return result;
}

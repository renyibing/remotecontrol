#ifndef SSL_VERIFIER_H_
#define SSL_VERIFIER_H_

#include <string>

// openssl
#include <openssl/ssl.h>

// Class for performing SSL certificate verification
class SSLVerifier {
 public:
  static bool VerifyX509(X509* x509, STACK_OF(X509) * chain);

 private:
  // Add PEM format root certificates
  static bool AddCert(const std::string& pem, X509_STORE* store);
  // Add WebRTC's built-in root certificates
  static bool LoadBuiltinSSLRootCertificates(X509_STORE* store);
};

#endif  // SSL_VERIFIER_H_

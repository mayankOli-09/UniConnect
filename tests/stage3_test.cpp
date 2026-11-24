// Stage 3a check: crypto, password hashing, JWT and the login/register logic.
#include <iostream>
#include <string>

#include "auth/JwtService.h"
#include "auth/PasswordHasher.h"
#include "database/InMemoryUserRepository.h"
#include "services/AuthService.h"
#include "utils/Crypto.h"
#include "utils/Json.h"

using namespace uniconnect;
using namespace uniconnect::utils;

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::cout << "  FAIL at line " << __LINE__ << "\n"; ++failures; } } while (0)

static std::string bytes(unsigned char v, size_t n) { return std::string(n, static_cast<char>(v)); }

int main() {
    std::cout << "1. SHA-256 / HMAC / PBKDF2 against published test vectors\n";
    CHECK(crypto::toHex(crypto::sha256("abc")) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(crypto::toHex(crypto::sha256("")) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(crypto::toHex(crypto::sha256(std::string(1000000, 'a'))) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    CHECK(crypto::toHex(crypto::hmacSha256(bytes(0x0b, 20), "Hi There")) == "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");
    CHECK(crypto::toHex(crypto::hmacSha256("Jefe", "what do ya want for nothing?")) == "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
    CHECK(crypto::toHex(crypto::hmacSha256(bytes(0xaa, 131), "Test Using Larger Than Block-Size Key - Hash Key First")) == "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");
    CHECK(crypto::toHex(crypto::pbkdf2Sha256("password", "salt", 1, 32)) == "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b");
    CHECK(crypto::toHex(crypto::pbkdf2Sha256("password", "salt", 2, 32)) == "ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43");
    CHECK(crypto::toHex(crypto::pbkdf2Sha256("password", "salt", 4096, 32)) == "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a");
    CHECK(crypto::toHex(crypto::pbkdf2Sha256("passwordPASSWORDpassword", "saltSALTsaltSALTsaltSALTsaltSALTsalt", 4096, 40)) ==
          "348c89dbcbd32b2f32d814b8116e84cf2b17347ebc1800181c4e2a1fb8dd53e1c635518c7dac47e9");

    std::cout << "2. hex / base64url / random\n";
    std::string raw;
    CHECK(crypto::fromHex("00ff10Ab", raw) && crypto::toHex(raw) == "00ff10ab");
    CHECK(!crypto::fromHex("abc", raw) && !crypto::fromHex("zz", raw));
    for (const std::string& s : {std::string(""), std::string("f"), std::string("fo"), std::string("foo"), std::string("foob"), std::string("\xff\xfe\xfd")}) {
        std::string back;
        CHECK(crypto::base64UrlDecode(crypto::base64UrlEncode(s), back) && back == s);
    }
    CHECK(crypto::base64UrlEncode("foobar") == "Zm9vYmFy");
    std::string tmp;
    CHECK(!crypto::base64UrlDecode("ab$d", tmp));
    CHECK(crypto::randomBytes(16).size() == 16 && crypto::randomBytes(16) != crypto::randomBytes(16));
    CHECK(crypto::constantTimeEquals("abc", "abc") && !crypto::constantTimeEquals("abc", "abd") && !crypto::constantTimeEquals("abc", "ab"));

    std::cout << "3. password hashing\n";
    const std::string h1 = auth::PasswordHasher::hash("Password@123");
    const std::string h2 = auth::PasswordHasher::hash("Password@123");
    CHECK(h1.rfind("pbkdf2_sha256$100000$", 0) == 0);
    CHECK(h1 != h2);                                                  // random salt
    CHECK(auth::PasswordHasher::verify("Password@123", h1));
    CHECK(!auth::PasswordHasher::verify("password@123", h1));
    CHECK(!auth::PasswordHasher::verify("Password@123", "hash123")); // old placeholder values never match
    CHECK(!auth::PasswordHasher::verify("x", "pbkdf2_sha256$99999999999$aa$bb"));
    CHECK(!auth::PasswordHasher::verify("x", "pbkdf2_sha256$1$zz$bb"));
    CHECK(!auth::PasswordHasher::verify("x", ""));

    std::cout << "4. JWT\n";
    // Reference token from jwt.io (secret "your-256-bit-secret")
    auth::JwtService ref("your-256-bit-secret");
    CHECK(ref.signPayload("{\"sub\":\"1234567890\",\"name\":\"John Doe\",\"iat\":1516239022}") ==
          "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiaWF0IjoxNTE2MjM5MDIyfQ.SflKxwRJSMeKKF2QT4fwpMeJf36POk6yJV_adQssw5c");
    auth::JwtService jwt("a-long-test-secret-value");
    auth::JwtClaims c;
    const std::string tok = jwt.issue(42, "FACULTY", 3600);
    CHECK(jwt.verify(tok, c) && c.user_id == 42 && c.role == "FACULTY" && c.expires_at > c.issued_at);
    CHECK(!jwt.verify(jwt.issue(42, "STUDENT", -10), c));                         // expired
    CHECK(!auth::JwtService("another-secret-value!!").verify(tok, c));            // wrong secret
    {   // tampered payload (role changed), signature left as is
        const size_t d1 = tok.find('.'), d2 = tok.find('.', d1 + 1);
        std::string forged;
        crypto::base64UrlDecode(tok.substr(d1 + 1, d2 - d1 - 1), forged);
        const size_t p = forged.find("FACULTY");
        forged.replace(p, 7, "ADMIN");
        CHECK(!jwt.verify(tok.substr(0, d1 + 1) + crypto::base64UrlEncode(forged) + tok.substr(d2), c));
    }
    {   // alg=none attack
        const std::string none = crypto::base64UrlEncode("{\"alg\":\"none\",\"typ\":\"JWT\"}") + "." +
                                 crypto::base64UrlEncode("{\"sub\":\"1\",\"role\":\"ADMIN\",\"exp\":99999999999}") + ".";
        CHECK(!jwt.verify(none, c));
    }
    CHECK(!jwt.verify("", c) && !jwt.verify("a.b", c) && !jwt.verify("a.b.c.d", c) && !jwt.verify("...", c));

    std::cout << "5. register\n";
    database::InMemoryUserRepository repo;
    services::AuthConfig cfg;
    cfg.jwt_secret = "unit-test-secret-0123456789";
    cfg.max_failed_logins = 5;
    services::AuthService svc(repo, cfg);

    auto reg = svc.registerUser("  Asha Rao ", "  Asha.Rao@Example.COM ", "Password@123", models::UserRole::STUDENT, "1");
    CHECK(reg.success);
    CHECK(reg.user.email == "asha.rao@example.com" && reg.user.name == "Asha Rao");
    CHECK(reg.user.student_id > 0 && reg.user.section_id == 1 && reg.user.department_id == 1);
    CHECK(reg.user.password_hash.empty());
    auto claims = svc.verifyToken(reg.token);
    CHECK(claims && claims->user_id == reg.user.id && claims->role == "STUDENT");
    const std::string json = reg.toJson().serialize(false);
    CHECK(json.find("password") == std::string::npos);
    CHECK(json.find("\"student_id\":") != std::string::npos && json.find("\"token\":") != std::string::npos);

    CHECK(!svc.registerUser("Asha Again", "ASHA.RAO@example.com", "Password@123", models::UserRole::STUDENT, "1").success);   // duplicate, case-insensitive
    CHECK(!svc.registerUser("A", "a@b.co", "Password@123", models::UserRole::STUDENT, "1").success);                        // name too short
    CHECK(!svc.registerUser("Bob Lee", "bob", "Password@123", models::UserRole::STUDENT, "1").success);                      // bad email
    CHECK(!svc.registerUser("Bob Lee", "bob@x", "Password@123", models::UserRole::STUDENT, "1").success);
    CHECK(!svc.registerUser("Bob Lee", "bob@@x.com", "Password@123", models::UserRole::STUDENT, "1").success);
    CHECK(!svc.registerUser("Bob Lee", "b o b@x.com", "Password@123", models::UserRole::STUDENT, "1").success);
    CHECK(!svc.registerUser("Bob Lee", "bob@x.com", "short", models::UserRole::STUDENT, "1").success);                       // weak password
    CHECK(!svc.registerUser("Bob Lee", "bob@x.com", "Password@123", models::UserRole::STUDENT, "99").success);               // unknown department
    CHECK(!svc.registerUser("Bob Lee", "bob@x.com", "Password@123", models::UserRole::STUDENT, "dept-cse-01").success);     // old text id
    CHECK(!svc.registerUser("Bob Lee", "bob@x.com", "Password@123", models::UserRole::STUDENT, "").success);
    auto priv = svc.registerUser("Eve Admin", "eve@x.com", "Password@123", models::UserRole::ADMIN, "1");
    CHECK(!priv.success && priv.error_message.find("administrator") != std::string::npos);                                   // no self-service admin
    CHECK(!svc.registerUser("Fay Prof", "fay@x.com", "Password@123", models::UserRole::FACULTY, "1").success);
    CHECK(svc.registerUser("Dee Two", "dee@x.com", "Password@123", models::UserRole::STUDENT, "2").user.section_id == 3);     // dept 2 -> section 3

    services::AuthConfig open = cfg;
    open.allow_privileged_signup = true;
    database::InMemoryUserRepository repo2;
    services::AuthService svc2(repo2, open);
    auto fac = svc2.registerUser("Prof X", "x@uni.edu", "Password@123", models::UserRole::FACULTY, "1");
    CHECK(fac.success && fac.user.faculty_id > 0 && fac.user.student_id == 0);
    CHECK(svc2.registerUser("Root", "root@uni.edu", "Password@123", models::UserRole::ADMIN, "1").success);

    std::cout << "6. login\n";
    auto ok = svc.login("ASHA.rao@example.com ", "Password@123");
    CHECK(ok.success && ok.user.id == reg.user.id && svc.verifyToken(ok.token));
    auto wrong = svc.login("asha.rao@example.com", "wrong-password");
    auto ghost = svc.login("nobody@example.com", "Password@123");
    CHECK(!wrong.success && !ghost.success && wrong.error_message == ghost.error_message);   // same message: no account enumeration
    CHECK(!svc.login("", "").success);

    std::cout << "7. lockout after repeated failures\n";
    for (int i = 0; i < 4; ++i) svc.login("dee@x.com", "bad-pass-" + std::to_string(i));
    CHECK(svc.login("dee@x.com", "Password@123").success);        // 4 failures then the right password: still fine
    for (int i = 0; i < 5; ++i) svc.login("dee@x.com", "bad-pass-" + std::to_string(i));
    auto locked = svc.login("dee@x.com", "Password@123");
    CHECK(!locked.success && locked.error_message.find("Too many") != std::string::npos);   // right password now refused
    CHECK(svc.login("asha.rao@example.com", "Password@123").success);                       // other accounts unaffected

    if (failures == 0) std::cout << "\nSTAGE 3a OK\n";
    else std::cout << "\nSTAGE 3a FAILED (" << failures << " checks)\n";
    return failures == 0 ? 0 : 1;
}

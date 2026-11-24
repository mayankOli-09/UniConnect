// Prints a password hash and the SQL to store it.   Usage: hashpw.exe "Password@123"
#include <iostream>
#include "auth/PasswordHasher.h"

int main(int argc, char** argv) {
    if (argc < 2) { std::cout << "Usage: hashpw.exe <password>\n"; return 1; }
    const std::string h = uniconnect::auth::PasswordHasher::hash(argv[1]);
    std::cout << h << "\n\nSQL:\nUPDATE users SET password_hash = '" << h << "' WHERE email = 'someone@example.com';\n";
    return 0;
}

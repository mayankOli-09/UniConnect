&lt;div align=&quot;center&quot;&gt;

&lt;h1 align=&quot;center&quot;&gt;🎓 UniConnect — Integrated University Communication &amp; Collaboration Platform&lt;/h1&gt;

**One hierarchy. One source of truth. Role-based, real-time communication.**

*Hierarchy-Aware Groups • Role-Based Access Control (RBAC) • Real-Time WebSocket Chat • Announcements & Notifications • C++20 Concurrency Engine*

[](https://isocpp.org)
[](https://react.dev)
[](https://www.mysql.com)
[](#-quick-start-windows)
[](#-current-status)
[License](https://www.google.com/search?q=LICENSE)

&lt;/div&gt;

---

## 🎯 What is UniConnect?

**UniConnect** is a real-time university communication platform in which chat rooms, announcement channels, and notification groups are **generated automatically from the university's academic hierarchy, faculty-to-section assignments, and live enrollment records**, with **strict backend-enforced Role-Based Access Control (RBAC)**.

> ⚠️ **The problem:** University communication is often scattered across LMS announcement boards, email listservs, and third-party chat tools (Slack, Teams, Discord). None of these automatically reflect the real organizational structure (university → department → program → section → course) or stay in sync when students enroll/drop or faculty are assigned to sections.

UniConnect treats the academic hierarchy and RBAC assignments as the **single source of truth** for every communication group, backed by a **C++20 multithreaded server** and **MySQL 8.0**.

**Key Capabilities:**
- **Role-Based Access Control (RBAC)** enforced across Database, C++ Backend (REST + WebSockets), and React Frontend:
  - **Admin:** Full university-wide visibility across all sections, all non-private groups (including student-created groups), a dedicated **Faculty assignments** tab (auto-adding assigned faculty to section groups), and an **Academics** management panel to add Students, Faculty, Departments/Sections, and Subjects.
  - **Faculty:** Scoped access to only their assigned sections (`faculty_sections`), groups belonging to those sections, and students in those sections.
  - **Student:** Scoped access to their own section, enrolled course/section/department groups, and groups they create.
- **Self-Registration by Role:** New users can sign up as either **Student** or **Faculty** (Admin accounts remain restricted).
- **Real-Time Chat & Presence:** Custom WebSocket server (`/ws`) supporting live messaging, typing indicators, online presence, unread counts, mute controls, and custom group creation.
- **Announcements & Notifications:** Targeted announcements and university-wide admin notices delivered in real time.
- **C++20 Concurrency Engine:** Non-blocking `WSAPoll` socket event loop, bounded producer-consumer task queue, fixed-size thread pool, and pluggable scheduling policies (`FCFS`, `SJF`, `ROUND_ROBIN`, `PRIORITY`).

---

## 📌 Current Status

| Area | Status | Notes |
|---|---|---|
| MySQL schema + seed + RBAC migrations | ✅ Done | `uniconnect.sql`, `demo_passwords.sql`, `part2_chat.sql`, `part3_notices.sql`, `part4_rbac.sql` |
| HTTP & WebSocket server (Windows `WSAPoll`) | ✅ Done | Keep-alive, pipelining, CORS, size limits, `/ws` live frames |
| Authentication: register (Student/Faculty), login, JWT | ✅ Done | PBKDF2-SHA256 passwords, HS256 tokens, login lockout |
| Concurrency engine (thread pool, task queue, scheduler) | ✅ Done | Wired into chat message delivery & announcement dispatch |
| Role-Based Access Control (Admin / Faculty / Student) | ✅ Done | Enforced on sections, groups, messages, directory, and WebSockets |
| Academics, Enrollment & Admin Management Panel | ✅ Done | Hierarchy tree, enroll/drop, plus Admin add Student/Faculty/Department/Subject |
| Admin **Faculty assignments** tab | ✅ Done | Assign/remove faculty to sections with automatic section-group membership sync |
| Groups, Real-Time Chat & Notifications | ✅ Done | REST + WebSocket push, unread tracking, mute, typing, presence |
| Redis cache, MinIO file storage, Nginx | 🗓️ Planned | Optional future scaling extensions |

---

## 🚀 Quick Start (Windows)

UniConnect targets **Windows 10/11 (x64)**. Everything below runs in **PowerShell**; no WSL, Linux, or Docker is needed.

### 1. Install the tools (one time)

| Tool | Install |
|---|---|
| Visual Studio 2022 Build Tools (C++ compiler) | `winget install Microsoft.VisualStudio.2022.BuildTools --override "--wait --passive --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"` |
| CMake 3.20+ | `winget install Kitware.CMake` |
| Node.js 18+ (LTS) | `winget install OpenJS.NodeJS.LTS` |
| MySQL **8.0** | See step 2 |

After installing CMake or Node, open a new PowerShell window and verify:

```powershell
cmake --version
node --version
```

### 2. Install MySQL 8.0 (portable ZIP)

The ZIP build includes the C client library the backend links against (`include\mysql.h`, `lib\libmysql.lib`).

```powershell
$ProgressPreference = 'SilentlyContinue'
$zip = "$env:TEMP\mysql-8.0.46-winx64.zip"
Invoke-WebRequest -Uri "https://cdn.mysql.com/Downloads/MySQL-8.0/mysql-8.0.46-winx64.zip" -OutFile $zip
Expand-Archive -Path $zip -DestinationPath D:\ -Force
Rename-Item D:\mysql-8.0.46-winx64 D:\mysql
```

Create the data folder (once) and start the server:

```powershell
D:\mysql\bin\mysqld.exe --initialize-insecure --console --basedir=D:\mysql --datadir=D:\mysql-data

Start-Process -FilePath D:\mysql\bin\mysqld.exe -ArgumentList "--console","--basedir=D:\mysql","--datadir=D:\mysql-data","--bind-address=127.0.0.1","--mysqlx=OFF"
Start-Sleep -Seconds 10
D:\mysql\bin\mysql.exe -u root -e "SELECT VERSION();"
```

### 3. Get the code

```powershell
git clone https://github.com/<your-org>/UniConnect.git
cd UniConnect
```

### 4. Create the database & run migrations

From the project folder:

```powershell
D:\mysql\bin\mysql.exe -u root -e "CREATE DATABASE IF NOT EXISTS uniconnect CHARACTER SET utf8mb4;"
D:\mysql\bin\mysql.exe -u root uniconnect -e "source database/uniconnect.sql"
D:\mysql\bin\mysql.exe -u root uniconnect -e "source database/part2_chat.sql"
D:\mysql\bin\mysql.exe -u root uniconnect -e "source database/part3_notices.sql"
D:\mysql\bin\mysql.exe -u root uniconnect -e "source database/part4_rbac.sql"

# Create the backend database account
D:\mysql\bin\mysql.exe -u root -e "CREATE USER IF NOT EXISTS 'uniconnect'@'localhost' IDENTIFIED BY 'UniConnect#Dev2026'; GRANT SELECT, INSERT, UPDATE, DELETE ON uniconnect.* TO 'uniconnect'@'localhost';"

# Set working demo password (Password@123) for the 7 seeded users
D:\mysql\bin\mysql.exe -u root -e "source database/demo_passwords.sql"
```

### 5. Build the backend

```powershell
cmake -S . -B build-win -G "Visual Studio 17 2022" -A x64
cmake --build build-win --config Release
```

If MySQL is not in `D:\mysql`, pass `-DMYSQL_ROOT="C:/path/to/mysql"` to the first `cmake` command. The build copies `libmysql.dll` and its OpenSSL DLLs next to `uniconnect.exe` automatically.

### 6. Start everything (One Command)

Install frontend dependencies once:

```powershell
cd Frontend
npm install
cd ..
```

Then launch MySQL, the C++ backend, and the React frontend together:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\start-all.ps1
```

This opens <http://localhost:5173> automatically.

**To stop everything:** press `Ctrl+C` in the backend and frontend PowerShell windows, then stop MySQL with `D:\mysql\bin\mysqladmin.exe -u root shutdown`.

---

## 👤 Demo Accounts

After running `database/demo_passwords.sql`, all seeded accounts use the password **`Password@123`**:

| Role | Name | Email |
|---|---|---|
| Student | Rahul Sharma | `rahul@uniconnect.com` |
| Student | Priya Singh | `priya@uniconnect.com` |
| Student | Aman Verma | `aman@uniconnect.com` |
| Student | Neha Gupta | `neha@uniconnect.com` |
| Faculty | Dr. Rajesh Kumar | `rajesh@uniconnect.com` |
| Faculty | Dr. Anjali Mehta | `anjali@uniconnect.com` |
| Admin | Admin User | `admin@uniconnect.com` |

---

## 🧰 Configuration

`scripts/run-backend.ps1` sets local development defaults for these environment variables:

| Variable | Default | Purpose |
|---|---|---|
| `MYSQL_PASSWORD` | *(required)* | Password of the database user |
| `MYSQL_USER` | `uniconnect` | Database user |
| `MYSQL_HOST` / `MYSQL_PORT` | `127.0.0.1` / `3306` | Database address |
| `MYSQL_DB` | `uniconnect` | Database name |
| `PORT` | `8080` | HTTP/WebSocket port of the backend |
| `JWT_SECRET` | random per run | Token signing secret (**set a long random value outside local development**) |
| `TOKEN_TTL_HOURS` | `24` | Login token lifetime |
| `WORKER_THREADS` | `4` | Thread pool worker count (1–64) |
| `SCHEDULER_POLICY` | `FCFS` | `FCFS`, `SJF`, `ROUND_ROBIN`, or `PRIORITY` |
| `TIME_QUANTUM_MS` | `25` | Quantum for `ROUND_ROBIN` |
| `QUEUE_CAPACITY` | `1000` | Bounded task queue capacity |
| `UNICONNECT_ALLOW_PRIVILEGED_SIGNUP` | off | Set to `1` to allow self-registration as `ADMIN` (demos only; `STUDENT` and `FACULTY` are enabled by default) |
| `UNICONNECT_LOG` | info | Set to `debug` for verbose logs |

> ⚠️ `scripts/run-backend.ps1` contains a local development password and secret. Change them before any non-local deployment.

---

## 🔌 API Reference

All responses are JSON. Errors use `{"error": "message"}`. Authenticated routes require `Authorization: Bearer <token>`.

### Authentication
| Method | Path | Description |
|---|---|---|
| `GET` | `/health` | Returns `{"status":"ok","service":"UniConnect Backend"}` |
| `POST` | `/api/auth/register` | Register a new `STUDENT` or `FACULTY` account |
| `POST` | `/api/auth/login` | Sign in and receive JWT + user profile |

### Sections, RBAC & Academics
| Method | Path | Access | Description |
|---|---|---|---|
| `GET` | `/api/sections` | Authenticated | Returns sections visible to caller's role (Admin: all; Faculty: assigned via `faculty_sections`; Student: own section) |
| `GET` | `/api/sections/{id}/groups` | Section RBAC | Returns groups belonging to section `{id}` |
| `GET` | `/api/sections/{id}/students` | Section RBAC | Returns students in section `{id}` |
| `GET` | `/api/faculty-sections` | Admin only | Lists all faculty, sections, and current `faculty_sections` assignments |
| `POST` | `/api/faculty-sections` | Admin only | Assigns a faculty member to a section and adds them to the section group |
| `POST` | `/api/faculty-sections/remove` | Admin only | Removes a faculty-section assignment and section group membership |
| `GET` | `/api/courses` | Authenticated | Lists all courses |
| `GET` | `/api/hierarchy` | Authenticated | Role-filtered university → department → program → section → course tree |
| `POST` | `/api/enroll` | Student / Admin | Enroll in a course/section and sync group memberships |
| `POST` | `/api/drop` | Student / Admin | Drop a course/section and sync group memberships |
| `POST` | `/api/admin/students` | Admin only | Create a student account, assign section/course, and join groups |
| `POST` | `/api/admin/faculty` | Admin only | Create a faculty account and optionally assign department/subject |
| `POST` | `/api/admin/departments` | Admin only | Create a department, program, and sections |
| `POST` | `/api/admin/subjects` | Admin only | Create a subject/course and optionally assign faculty |

### Chat, Groups, Notifications & WebSocket
| Method | Path | Description |
|---|---|---|
| `GET` | `/api/groups` | Lists groups accessible to the caller's role |
| `GET` | `/api/groups/{id}` | Group details and member list (RBAC enforced) |
| `POST` | `/api/groups` | Create a custom group (tagged with `section_id`) |
| `GET` / `POST` | `/api/groups/{id}/messages` | Fetch paginated messages or send a message |
| `POST` | `/api/groups/{id}/leave` | Leave a user-created group |
| `POST` | `/api/groups/{id}/mute` | Mute or unmute a group |
| `POST` | `/api/groups/{id}/read` | Update read cursor |
| `GET` | `/api/users?q=` | Role-scoped user directory for starting chats |
| `POST` | `/api/chats/direct` | Start or retrieve a 1-on-1 direct chat |
| `GET` | `/api/notifications` | Fetch notifications and unread count |
| `POST` | `/api/notifications/read` | Mark notifications as read |
| `POST` | `/api/announcements` | Post a group announcement (Faculty / Admin) |
| `POST` | `/api/notices` | Broadcast a university notice (Admin only) |
| `GET` | `/ws` | WebSocket upgrade for live messages, typing, presence, and notifications |

---

## 🔒 Security Notes

- **Least-Privilege RBAC:** Section and group permissions are enforced on the server for every REST endpoint and WebSocket action (`subscribe`, `typing`, `send_message`), never solely in the UI.
- **Password Hashing:** Salted **PBKDF2-HMAC-SHA256** (100,000 iterations), never stored in plain text.
- **JWT Verification:** Standard **HS256 JWTs** with expiry; forged payloads and `alg: none` are rejected.
- **SQL Injection Protection:** Every query uses **prepared statements with bound parameters**.
- **Atomic Transactions:** Account creation, enrollments, faculty assignments, and entity creation execute inside MySQL transactions.
- **Brute-Force Protection:** Login locks out after 5 consecutive failures for 15 minutes.

---

## 🧪 Tests

The CMake build produces stage tests in `build-win\Release\`:

| Program | What it checks | Needs MySQL |
|---|---|---|
| `stage1_test.exe` – `stage11_test.exe` | JSON, logger, thread pool, scheduler, HTTP parser, crypto/JWT, auth, chat, notifications, WebSockets | No |
| `stage3b_test.exe`, `stage5b_test.exe`, `stage6b_test.exe`, `stage8b_test.exe`, `stage10b_test.exe`, `stage11b_test.exe` | Real MySQL database integration tests | **Yes** |

```powershell
.\build-win\Release\stage1_test.exe
.\build-win\Release\stage3_test.exe
.\build-win\Release\stage4_test.exe
```

A helper tool, `hashpw.exe "<password>"`, prints a PBKDF2 password hash and the SQL to store it.

---

## 🏗️ Project Structure

```
UniConnect/
├── CMakeLists.txt                   ← builds the backend, tests, and tools
├── database/
│   ├── uniconnect.sql               ← core schema + seed data (MySQL 8.0)
│   ├── part2_chat.sql               ← chat mute & read-cursor migration
│   ├── part3_notices.sql            ← announcements & notices migration
│   ├── part4_rbac.sql               ← faculty_sections RBAC migration
│   └── demo_passwords.sql           ← demo password for seeded users
├── include/                         ← C++ headers (auth, concurrency, database, http, models, routes, server, services, utils, websocket)
├── src/                             ← C++20 backend implementation
├── tests/                           ← stage unit & integration tests
├── tools/hashpw.cpp                 ← password hash generator
├── scripts/
│   ├── start-all.ps1                ← starts MySQL, backend, and frontend
│   └── run-backend.ps1              ← starts MySQL + backend
└── Frontend/                        ← React 18 + TypeScript + Vite + Tailwind
    └── src/
        ├── api/                     ← REST and WebSocket client + normalizers
        ├── chat/                    ← ChatContext provider
        ├── components/              ← Layout, NewChat, Toasts, shared UI
        └── pages/                   ← Login, Chat, Notifications, Academics, FacultyAssignments
```

---

## 🛠️ Tech Stack

| Layer | Technology |
|---|---|
| **Backend language** | C++20 (MSVC, Visual Studio 2022) |
| **Backend I/O** | Custom non-blocking socket event loop using Windows `WSAPoll` |
| **Database** | MySQL 8.0 (`libmysql`, prepared statements, transactions) |
| **Authentication** | JWT (HS256) and PBKDF2-SHA256 via Windows CNG |
| **Frontend** | React 18, TypeScript, Vite, Tailwind CSS |
| **Build** | CMake |

---

## 📸 Architecture

```
        Browser: React + TypeScript SPA  (http://localhost:5173)
                          │  /api  and  /ws   (Vite dev proxy)
                          ▼
        C++20 backend (http://localhost:8080)
        WSAPoll event loop ─► HTTP parser ─► Router ─► AuthGuard (JWT + RBAC) ─► Services
                                                                                      │
                                       ┌──────────────────────────────────────────────┴──────────────────┐
                                       ▼                                                                 ▼
                           MySQL 8.0 (prepared statements,                                   Concurrency engine
                           transactions, faculty_sections)                                   task queue ─► thread pool ─► scheduler
```

---

&lt;div align=&quot;center&quot;&gt;

⭐ **Star this repository to follow UniConnect's progress.**

*One Hierarchy • One Platform • Real-Time, Always in Sync*

Made with ❤️ using **C++20** • **React** • **MySQL**

&lt;/div&gt;

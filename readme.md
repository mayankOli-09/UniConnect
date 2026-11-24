<div align="center">

<h1 align="center">🎓 UniConnect — Integrated University Communication & Collaboration Platform</h1>

**One hierarchy. One source of truth. Role-based, real-time communication.**

*Hierarchy-Aware Groups • Role-Based Access Control (RBAC) • Real-Time WebSocket Chat • Announcements & Notifications • C++20 Concurrency Engine*

[![C++](https://img.shields.io/badge/C%2B%2B-20-blue?logo=c%2B%2B&logoColor=white)](https://isocpp.org)
[![React](https://img.shields.io/badge/React-TypeScript-61DAFB?logo=react&logoColor=black)](https://react.dev)
[![MySQL](https://img.shields.io/badge/MySQL-8.0-4479A1?logo=mysql&logoColor=white)](https://www.mysql.com)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%2F11-0078D6?logo=windows&logoColor=white)](#-quick-start-windows)
[![Status](https://img.shields.io/badge/Status-Feature%20complete-brightgreen)](#-current-status)
[![License](https://img.shields.io/badge/License-MIT-lightgrey)](LICENSE)

</div>

---

## 🎯 What is UniConnect?

**UniConnect** is a real-time university communication platform in which chat rooms, announcement channels, and notification groups are **generated automatically from the university's academic hierarchy, faculty-to-section assignments, and live enrollment records**, with **strict backend-enforced Role-Based Access Control (RBAC)**.

> ⚠️ **The problem:** University communication is often scattered across LMS announcement boards, email listservs, and third-party chat tools (Slack, Teams, Discord). None of these automatically reflect the real organizational structure (university → department → program → section → course) or stay in sync when students enroll or drop courses, or when faculty are assigned to sections.

UniConnect treats the academic hierarchy and RBAC assignments as the **single source of truth** for every communication group, backed by a **C++20 multithreaded server** and **MySQL 8.0**.

**For students, faculty and admins it provides:**
- **Role-Based Access Control (RBAC)**, enforced across the database, the C++ backend (REST and WebSockets) and the React frontend:
  - **Admin:** university-wide visibility across all sections and all non-private groups (including student-created groups), a dedicated **Faculty assignments** tab (assigned faculty are added to section groups automatically), and an **Academics** management panel to add students, faculty, departments/sections and subjects.
  - **Faculty:** scoped access to only their assigned sections (`faculty_sections`), the groups belonging to those sections, and the students in those sections.
  - **Student:** scoped access to their own section, their enrolled course/section/department groups, and the groups they create.
- **Self-registration by role:** new users can sign up as **Student** or **Faculty** (admin accounts stay restricted).
- **Real-time chat and presence:** a custom WebSocket server (`/ws`) with live messaging, typing indicators, online presence, unread counts, mute controls and custom group creation.
- **Announcements and notifications:** targeted announcements and university-wide admin notices delivered in real time.

**Under the hood it demonstrates:**
- A custom non-blocking `WSAPoll` socket event loop serving REST and WebSocket traffic on one port
- A bounded producer-consumer task queue feeding a fixed-size thread pool
- Pluggable CPU scheduling policies (`FCFS`, `SJF`, `ROUND_ROBIN`, `PRIORITY`)

---

## 📌 Current Status

All planned features are implemented. This table is the picture of what works today.

| Area | Status | Notes |
|---|---|---|
| MySQL schema + seed + RBAC migrations | ✅ Done | `uniconnect.sql`, `demo_passwords.sql`, `part2_chat.sql`, `part3_notices.sql`, `part4_rbac.sql` |
| HTTP & WebSocket server (Windows `WSAPoll`) | ✅ Done | Keep-alive, pipelining, CORS, size limits, `/ws` live frames |
| Authentication: register (Student/Faculty), login, JWT | ✅ Done | PBKDF2-SHA256 passwords, HS256 tokens, login lockout |
| Concurrency engine (thread pool, task queue, scheduler) | ✅ Done | Wired into chat message delivery and announcement dispatch |
| Role-Based Access Control (Admin / Faculty / Student) | ✅ Done | Enforced on sections, groups, messages, directory and WebSockets |
| Academics, enrollment and Admin management panel | ✅ Done | Hierarchy tree, enroll/drop, plus admin add Student / Faculty / Department / Subject |
| Admin **Faculty assignments** tab | ✅ Done | Assign/remove faculty to sections with automatic section-group membership sync |
| Groups, real-time chat and notifications | ✅ Done | REST + WebSocket push, unread tracking, mute, typing, presence |
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

After installing CMake or Node, **open a new PowerShell window** (or refresh PATH in the current one) and verify:

```powershell
$env:Path = [System.Environment]::GetEnvironmentVariable("Path","Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path","User")
cmake --version
node --version
```

### 2. Install MySQL 8.0 (portable ZIP, no installer)

The ZIP build includes the C client library the backend links against (`include\mysql.h`, `lib\libmysql.lib`).

```powershell
$ProgressPreference = 'SilentlyContinue'
$zip = "$env:TEMP\mysql-8.0.46-winx64.zip"
Invoke-WebRequest -Uri "https://cdn.mysql.com/Downloads/MySQL-8.0/mysql-8.0.46-winx64.zip" -OutFile $zip
Expand-Archive -Path $zip -DestinationPath D:\ -Force
Rename-Item D:\mysql-8.0.46-winx64 D:\mysql
```

> If that download address no longer works, get the "Windows (x86, 64-bit), ZIP Archive" for MySQL 8.0 from <https://dev.mysql.com/downloads/mysql/>, extract it to `D:\mysql`, and adjust the version in the commands.

Create the data folder (once), then start the server:

```powershell
D:\mysql\bin\mysqld.exe --initialize-insecure --console --basedir=D:\mysql --datadir=D:\mysql-data

Start-Process -FilePath D:\mysql\bin\mysqld.exe -ArgumentList "--console","--basedir=D:\mysql","--datadir=D:\mysql-data","--bind-address=127.0.0.1","--mysqlx=OFF"
Start-Sleep -Seconds 10
D:\mysql\bin\mysql.exe -u root -e "SELECT VERSION();"
```

> `--initialize-insecure` gives the local `root` account an empty password. That is acceptable only because the server is bound to `127.0.0.1` on your own machine. Never do this on a shared or public server.

### 3. Get the code

```powershell
git clone https://github.com/<your-org>/UniConnect.git
cd UniConnect
```

(No Git? Use **Code → Download ZIP** on GitHub and extract it.)

### 4. Create the database and run the migrations

From the project folder:

```powershell
D:\mysql\bin\mysql.exe -u root -e "CREATE DATABASE IF NOT EXISTS uniconnect CHARACTER SET utf8mb4;"
D:\mysql\bin\mysql.exe -u root uniconnect -e "source database/uniconnect.sql"
D:\mysql\bin\mysql.exe -u root uniconnect -e "source database/part2_chat.sql"
D:\mysql\bin\mysql.exe -u root uniconnect -e "source database/part3_notices.sql"
D:\mysql\bin\mysql.exe -u root uniconnect -e "source database/part4_rbac.sql"

# A limited account for the backend (replace the password with your own)
D:\mysql\bin\mysql.exe -u root -e "CREATE USER IF NOT EXISTS 'uniconnect'@'localhost' IDENTIFIED BY 'UniConnect#Dev2026'; GRANT SELECT, INSERT, UPDATE, DELETE ON uniconnect.* TO 'uniconnect'@'localhost';"

# Give the 7 seeded users a working demo password (Password@123)
D:\mysql\bin\mysql.exe -u root -e "source database/demo_passwords.sql"
```

The seeded accounts in the SQL dump only have placeholder password values, so `demo_passwords.sql` is what makes them able to log in. It is for local demos only.

### 5. Build the backend

```powershell
cmake -S . -B build-win -G "Visual Studio 17 2022" -A x64
cmake --build build-win --config Release
```

If MySQL is not in `D:\mysql`, add `-DMYSQL_ROOT="C:/path/to/mysql"` to the first command. The build copies `libmysql.dll` and its OpenSSL DLLs next to `uniconnect.exe` automatically.

### 6. Start everything (one command)

Install the frontend packages once:

```powershell
cd Frontend
npm install
cd ..
```

Then launch MySQL, the C++ backend and the React frontend together:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\start-all.ps1
```

This opens <http://localhost:5173> automatically. The backend window prints `Connected to MySQL database uniconnect` and then `UniConnect backend ready`. Windows Firewall may ask for permission the first time: choose **Allow access**.

Prefer to start the parts yourself? Use two windows:

```powershell
# Window 1: MySQL (if needed) + backend
powershell -ExecutionPolicy Bypass -File .\scripts\run-backend.ps1

# Window 2: frontend
cd Frontend
npm run dev
```

Check the backend from another window:

```powershell
Invoke-RestMethod http://localhost:8080/health
```

**To stop everything:** press `Ctrl+C` in the backend and frontend windows, then stop MySQL with `D:\mysql\bin\mysqladmin.exe -u root shutdown`.

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

The backend reads these environment variables. `scripts/run-backend.ps1` sets sensible local development values.

| Variable | Default | Purpose |
|---|---|---|
| `MYSQL_PASSWORD` | *(required)* | Password of the database user |
| `MYSQL_USER` | `uniconnect` | Database user |
| `MYSQL_HOST` / `MYSQL_PORT` | `127.0.0.1` / `3306` | Database address |
| `MYSQL_DB` | `uniconnect` | Database name |
| `PORT` | `8080` | HTTP and WebSocket port of the backend |
| `JWT_SECRET` | random per run | Token signing secret. **Set a long random value outside local development.** |
| `TOKEN_TTL_HOURS` | `24` | Login token lifetime |
| `WORKER_THREADS` | `4` | Thread pool worker count (1–64) |
| `SCHEDULER_POLICY` | `FCFS` | `FCFS`, `SJF`, `ROUND_ROBIN` or `PRIORITY` |
| `TIME_QUANTUM_MS` | `25` | Quantum for `ROUND_ROBIN` |
| `QUEUE_CAPACITY` | `1000` | Bounded task queue capacity |
| `UNICONNECT_ALLOW_PRIVILEGED_SIGNUP` | off | Set to `1` to allow self-registration as `ADMIN` (demos only; `STUDENT` and `FACULTY` are enabled by default) |
| `UNICONNECT_LOG` | info | Set to `debug` for verbose logs |

> ⚠️ `scripts/run-backend.ps1` contains a local development password and secret. Change them before any non-local deployment.

---

## 🔌 API Reference

All responses are JSON. Errors look like `{"error": "message"}`. Authenticated routes need the header `Authorization: Bearer <token>`. Common status codes: `400` invalid input, `401` not signed in or wrong credentials, `403` not allowed for your role, `404` not found, `409` already exists, `429` too many requests, `503` database unavailable.

### Authentication

| Method | Path | Description |
|---|---|---|
| `GET` | `/health` | Returns `{"status":"ok","service":"UniConnect Backend"}` |
| `POST` | `/api/auth/register` | Register a new `STUDENT` or `FACULTY` account |
| `POST` | `/api/auth/login` | Sign in and receive a JWT and the user profile |

### Sections, RBAC and Academics

| Method | Path | Access | Description |
|---|---|---|---|
| `GET` | `/api/sections` | Authenticated | Sections visible to the caller's role (Admin: all; Faculty: assigned via `faculty_sections`; Student: own section) |
| `GET` | `/api/sections/{id}/groups` | Section RBAC | Groups belonging to section `{id}` |
| `GET` | `/api/sections/{id}/students` | Section RBAC | Students in section `{id}` |
| `GET` | `/api/faculty-sections` | Admin only | Lists all faculty, sections and current `faculty_sections` assignments |
| `POST` | `/api/faculty-sections` | Admin only | Assigns a faculty member to a section and adds them to the section group |
| `POST` | `/api/faculty-sections/remove` | Admin only | Removes a faculty-section assignment and the section group membership |
| `GET` | `/api/courses` | Authenticated | Lists all courses |
| `GET` | `/api/hierarchy` | Authenticated | Role-filtered university → department → program → section → course tree |
| `POST` | `/api/enroll` | Student / Admin | Enroll in a course/section and sync group memberships |
| `POST` | `/api/drop` | Student / Admin | Drop a course/section and sync group memberships |
| `POST` | `/api/admin/students` | Admin only | Create a student account, assign section/course and join groups |
| `POST` | `/api/admin/faculty` | Admin only | Create a faculty account and optionally assign department/subject |
| `POST` | `/api/admin/departments` | Admin only | Create a department, program and sections |
| `POST` | `/api/admin/subjects` | Admin only | Create a subject/course and optionally assign faculty |

### Chat, Groups, Notifications and WebSocket

| Method | Path | Description |
|---|---|---|
| `GET` | `/api/groups` | Groups accessible to the caller's role |
| `GET` | `/api/groups/{id}` | Group details and member list (RBAC enforced) |
| `POST` | `/api/groups` | Create a custom group (tagged with `section_id`) |
| `GET` / `POST` | `/api/groups/{id}/messages` | Fetch paginated messages or send a message |
| `POST` | `/api/groups/{id}/leave` | Leave a user-created group |
| `POST` | `/api/groups/{id}/mute` | Mute or unmute a group |
| `POST` | `/api/groups/{id}/read` | Update the read cursor |
| `GET` | `/api/users?q=` | Role-scoped user directory for starting chats |
| `POST` | `/api/chats/direct` | Start or retrieve a 1-on-1 direct chat |
| `GET` | `/api/notifications` | Fetch notifications and the unread count |
| `POST` | `/api/notifications/read` | Mark notifications as read |
| `POST` | `/api/announcements` | Post a group announcement (Faculty / Admin) |
| `POST` | `/api/notices` | Broadcast a university notice (Admin only) |
| `GET` | `/ws` | WebSocket upgrade for live messages, typing, presence and notifications |

---

## 🔒 Security Notes

- **Least-privilege RBAC:** section and group permissions are enforced on the server for every REST endpoint and WebSocket action (`subscribe`, `typing`, `send_message`), never only in the UI.
- **Password hashing:** salted **PBKDF2-HMAC-SHA256** (100,000 iterations), never stored in plain text.
- **JWT verification:** standard **HS256 JWTs** with an expiry; forged payloads and `alg: none` tokens are rejected.
- **SQL injection protection:** every query uses **prepared statements with bound parameters**.
- **Atomic transactions:** account creation, enrollments, faculty assignments and entity creation run inside MySQL transactions, so a failure saves nothing.
- **Brute-force protection:** login locks out after 5 consecutive failures for 15 minutes.

---

## 🧪 Tests

The CMake build produces these test programs in `build-win\Release\`. Each ends with an `OK` line when everything passes.

| Program | What it checks | Needs MySQL |
|---|---|---|
| `stage1_test.exe` – `stage11_test.exe` | JSON, logger, thread pool, scheduler, HTTP parser, crypto/JWT, auth, chat, notifications, WebSockets | No |
| `stage3b_test.exe`, `stage5b_test.exe`, `stage6b_test.exe`, `stage8b_test.exe`, `stage10b_test.exe`, `stage11b_test.exe` | Real MySQL database integration tests | **Yes** |

```powershell
.\build-win\Release\stage1_test.exe
.\build-win\Release\stage3_test.exe
.\build-win\Release\stage4_test.exe
.\build-win\Release\stage3b_test.exe   # MySQL must be running with the schema loaded
```

The database tests use the development password from step 4 unless `MYSQL_PASSWORD` is set, and remove the rows they create. A helper tool, `hashpw.exe "<password>"`, prints a PBKDF2 password hash and the SQL to store it.

---

## 🏗️ Project Structure

```
UniConnect/
├── CMakeLists.txt                   ← builds the backend, tests and tools
├── database/
│   ├── uniconnect.sql               ← core schema + seed data (MySQL 8.0)
│   ├── part2_chat.sql               ← chat mute and read-cursor migration
│   ├── part3_notices.sql            ← announcements and notices migration
│   ├── part4_rbac.sql               ← faculty_sections RBAC migration
│   └── demo_passwords.sql           ← demo password for the seeded users
├── include/                         ← C++ headers (auth, concurrency, database, http, models, routes, server, services, utils, websocket)
├── src/                             ← C++20 backend implementation
├── tests/                           ← stage unit and integration tests
├── tools/hashpw.cpp                 ← password hash generator
├── scripts/
│   ├── start-all.ps1                ← starts MySQL, backend and frontend
│   └── run-backend.ps1              ← starts MySQL + the backend
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
| **Planned** | Redis (cache), MinIO (file storage), Nginx (reverse proxy) |

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

## 🧯 Troubleshooting

| Problem | Fix |
|---|---|
| `cmake` or `node` "is not recognized" | Open a **new** PowerShell window, or run the PATH refresh command from step 1 |
| `MYSQL_PASSWORD is not set` | Start through `scripts\run-backend.ps1` or `start-all.ps1`, or set `$env:MYSQL_PASSWORD = "..."` first |
| `Cannot connect to MySQL` | MySQL is not running. Start it with the `Start-Process ... mysqld.exe` command, then retry |
| Build error: MySQL client files not found | Pass `-DMYSQL_ROOT="C:/path/to/mysql"` to the first `cmake` command |
| `libmysql.dll` or another DLL is missing when starting | Re-run `cmake --build build-win --config Release`; the build copies the DLLs next to the `.exe` |
| `Cannot bind port 8080` | An older backend is still running. Run `Get-Process uniconnect -ErrorAction SilentlyContinue \| Stop-Process -Force`, or set `$env:PORT = "8081"` |
| Login says "Invalid email or password" for a demo user | `database/demo_passwords.sql` was not run |
| Login page shows a network error | The backend is not running |
| A page says "Endpoint not found" | The running backend is an old build. Stop it, rebuild, and start it again |
| Script blocked by execution policy | Run it as `powershell -ExecutionPolicy Bypass -File .\scripts\run-backend.ps1` |

---

## 🧪 Test Scenarios

| Scenario | Expected Result |
|---|---|
| Student enrolls in a new course | Communication groups update to include the student |
| Student drops a course | Student is removed from the corresponding group(s) |
| Admin assigns a faculty member to a section | Faculty gains access to that section and is added to its group |
| Faculty opens the section list | Only sections assigned through `faculty_sections` are shown |
| Student opens the section list | Only their own section is shown |
| Faculty or admin posts an announcement | Group members receive it in real time |
| Burst of concurrent chat messages | The queue processes all messages without loss or reordering |
| Invalid or expired JWT | Request rejected with an authentication error |
| User tries to subscribe to a group outside their scope | Rejected by the server (WebSocket and REST) |

---

## 🗺️ Roadmap

**Phase-I — Proposal & Design** ✅

**Phase-II — Development** ✅
- Database schema with chat, notice and RBAC migrations
- HTTP and WebSocket server, authentication, concurrency engine
- RBAC for Admin, Faculty and Student, including the Faculty assignments tab
- Academics and enrollment, Admin management panel
- Groups, real-time chat, announcements and notifications

**Phase-III — Optional extensions**
- Redis cache, MinIO file storage, Nginx reverse proxy
- Concurrent load testing, benchmarking and evaluation

---

## 📚 References

1. Silberschatz, Galvin, Gagne — *Operating System Concepts* (scheduling, synchronization, deadlock)
2. [MySQL 8.0 documentation](https://dev.mysql.com/doc/refman/8.0/en/) (C API, prepared statements, transactions)
3. [JWT introduction](https://jwt.io/introduction)
4. [RFC 6070 — PBKDF2 test vectors](https://www.rfc-editor.org/rfc/rfc6070) and [RFC 4231 — HMAC-SHA-256 test vectors](https://www.rfc-editor.org/rfc/rfc4231)
5. [Winsock `WSAPoll`](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsapoll)
6. [React](https://react.dev/), [Vite](https://vite.dev/) and [Tailwind CSS](https://tailwindcss.com/docs) documentation

---

## 🤝 Contributing

```bash
git checkout -b feature/your-feature-name
git commit -m "Add: your feature description"
git push origin feature/your-feature-name
# Open a Pull Request 🎉
```

---

<div align="center">

⭐ **Star this repository to follow UniConnect's progress.**

*One Hierarchy • One Platform • Real-Time, Always in Sync*

Made with ❤️ using **C++20** • **React** • **MySQL**

</div>

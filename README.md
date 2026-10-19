<div align="center">

<h1 align="center">🎓 UniConnect — Integrated University Communication & Collaboration Platform</h1>

**One hierarchy. One source of truth. Real-time communication.**

*Hierarchy-Aware Groups • Real-Time Chat • Announcements & Notifications • Concurrency Engine • CPU Scheduling • OS Live Dashboard*

[![C++](https://img.shields.io/badge/C%2B%2B-20-blue?logo=c%2B%2B&logoColor=white)](https://isocpp.org)
[![React](https://img.shields.io/badge/React-TypeScript-61DAFB?logo=react&logoColor=black)](https://react.dev)
[![MySQL](https://img.shields.io/badge/MySQL-8.0-4479A1?logo=mysql&logoColor=white)](https://www.mysql.com)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%2F11-0078D6?logo=windows&logoColor=white)](#-quick-start-windows)
[![Status](https://img.shields.io/badge/Status-Phase--II%20(Development)-orange)](#-current-status)
[![License](https://img.shields.io/badge/License-MIT-lightgrey)](LICENSE)

</div>

---

## 🎯 What is UniConnect?

**UniConnect** is a real-time university communication platform in which chat rooms, announcement channels, and notification groups are **generated automatically from the university's academic hierarchy and live enrollment records**, so membership is always correct, with no manual maintenance.

> ⚠️ **The problem:** University communication today is scattered across LMS announcement boards, email listservs, and third-party chat tools (Slack, Teams, Discord). None of these automatically reflect the real organizational structure (university → college → department → course → section) or stay in sync when students add/drop courses. The result: missed announcements, stale groups, and constant manual admin overhead.

UniConnect treats the academic hierarchy and enrollment data as the **single source of truth** for every communication group. It is also a working showcase of core **Operating Systems** concepts (multithreading, thread pools, producer-consumer queues, synchronization, CPU scheduling, deadlock handling) on a **C++20 backend**.

**For students and faculty it provides:**
- Automatically generated, always-accurate groups per course, section, and department
- Real-time chat over WebSockets
- Instant announcements and in-app notifications
- Role-based access for students, faculty, and admins

**Under the hood it demonstrates:**
- A custom non-blocking socket event loop handling REST (and, later, WebSocket) traffic
- A bounded producer-consumer task queue feeding a fixed-size thread pool
- Pluggable CPU scheduling policies (FCFS, SJF, Round Robin, Priority)
- An interactive **Scheduler lab** that animates how each policy orders the same processes

---

## 📌 Current Status

The project is under active development. This table is the honest picture of what works today.

| Area | Status | Notes |
|---|---|---|
| MySQL schema + seed data | ✅ Done | `database/uniconnect.sql`, 7 seeded users |
| HTTP server (Windows, `WSAPoll` event loop) | ✅ Done | Keep-alive, pipelining, CORS, size limits |
| Authentication: register, login, JWT | ✅ Done | PBKDF2-SHA256 passwords, HS256 tokens, login lockout |
| Concurrency engine (thread pool, task queue, scheduler) | ✅ Built and tested | Not yet wired into the request path |
| Frontend: sign-in / create account | ✅ Works end to end | |
| Frontend: **Scheduler lab** (policy simulator) | ✅ Works offline | Runs in the browser, no backend needed |
| Courses, hierarchy, enroll / drop | 🚧 Frontend ready, backend pending | |
| Groups and chat (REST + WebSocket) | 🚧 Frontend ready, backend pending | |
| Notifications and announcements | 🚧 Frontend ready, backend pending | |
| OS system monitor (live metrics) | 🚧 Frontend ready, backend pending | |
| Redis cache, MinIO file storage, Nginx | 🗓️ Planned | Not used by the current code |

After you sign in, the Chat, Notifications, Academics and System monitor pages will show errors until their backend endpoints are added. That is expected.

---

## 🚀 Quick Start (Windows)

UniConnect targets **Windows 10/11 (x64)** only. Everything below runs in **PowerShell**; no WSL, Linux, or Docker is needed.

### 1. Install the tools (one time)

| Tool | Install |
|---|---|
| Visual Studio 2022 Build Tools (C++ compiler) | `winget install Microsoft.VisualStudio.2022.BuildTools --override "--wait --passive --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"` |
| CMake 3.20+ | `winget install Kitware.CMake` |
| Node.js 18+ (LTS) | `winget install OpenJS.NodeJS.LTS` |
| MySQL **8.0** | See step 2 |

After installing CMake or Node, **open a new PowerShell window** (or refresh PATH in the current one):

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

### 4. Create the database

From the project folder:

```powershell
D:\mysql\bin\mysql.exe -u root -e "CREATE DATABASE IF NOT EXISTS uniconnect CHARACTER SET utf8mb4;"
D:\mysql\bin\mysql.exe -u root uniconnect -e "source database/uniconnect.sql"

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

### 6. Start the backend

The easiest way is the helper script. It starts MySQL if it is not running, sets the environment variables, and launches the server:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run-backend.ps1
```

You should see `Connected to MySQL database uniconnect` and then `UniConnect backend ready`. Leave this window open. Windows Firewall may ask for permission the first time: choose **Allow access**.

Check it from a second window:

```powershell
Invoke-RestMethod http://localhost:8080/health
```

Prefer to set things yourself? See [Configuration](#-configuration) and run `.\build-win\Release\uniconnect.exe`.

### 7. Start the frontend

In a second PowerShell window:

```powershell
cd frontend
npm install
npm run dev
```

Open <http://localhost:5173>. Use a demo account button on the sign-in page, or create a student account.

**To stop everything:** press `Ctrl+C` in the backend and frontend windows, then stop MySQL with `D:\mysql\bin\mysqladmin.exe -u root shutdown`.

---

## 👤 Demo Accounts

After running `database/demo_passwords.sql`, all use the password **`Password@123`**:

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

The backend reads these environment variables. `scripts/run-backend.ps1` sets sensible local values.

| Variable | Default | Purpose |
|---|---|---|
| `MYSQL_PASSWORD` | *(required)* | Password of the database user |
| `MYSQL_USER` | `uniconnect` | Database user |
| `MYSQL_HOST` / `MYSQL_PORT` | `127.0.0.1` / `3306` | Database address |
| `MYSQL_DB` | `uniconnect` | Database name |
| `PORT` | `8080` | HTTP port of the backend |
| `JWT_SECRET` | random per run | Token signing secret. If unset, every restart logs everyone out. **Set a long random value outside local development.** |
| `TOKEN_TTL_HOURS` | `24` | How long a login token lasts |
| `UNICONNECT_ALLOW_PRIVILEGED_SIGNUP` | off | Set to `1` to let anyone self-register as Faculty or Admin (demos only) |
| `UNICONNECT_LOG` | info | Set to `debug` for verbose logs |

> ⚠️ `scripts/run-backend.ps1` contains a local development password and secret. Change them, and do not reuse them anywhere real.

---

## 🔌 API Reference (implemented)

All responses are JSON. Errors look like `{"error": "message"}`.

| Method | Path | Description |
|---|---|---|
| `GET` | `/health` | Returns `{"status":"ok","service":"UniConnect Backend"}` |
| `POST` | `/api/auth/register` | Create a student account |
| `POST` | `/api/auth/login` | Sign in |

**Register**

```http
POST /api/auth/register
{ "name": "Asha Rao", "email": "asha@example.com", "password": "Password@123", "department_id": "1" }
```

- `department_id` is `1` (CSE), `2` (Mechanical) or `3` (ECE); a numeric or text value is accepted.
- `role` defaults to `STUDENT`. `FACULTY` and `ADMIN` return `403` unless `UNICONNECT_ALLOW_PRIVILEGED_SIGNUP=1`, so nobody can make themselves an administrator.
- A student gets a `students` row with an auto-generated roll number and the first section of their department. An optional `section_id` must belong to that department.
- Returns `201` with the same body as login.

**Login**

```http
POST /api/auth/login
{ "email": "rahul@uniconnect.com", "password": "Password@123" }
```

```json
{
  "success": true,
  "token": "eyJhbGciOi...",
  "token_type": "Bearer",
  "expires_in": 86400,
  "user": { "user_id": 1, "name": "Rahul Sharma", "email": "rahul@uniconnect.com",
            "role": "STUDENT", "department_id": 1, "student_id": 1, "section_id": 1 }
}
```

| Status | Meaning |
|---|---|
| `400` | Invalid or missing input (bad email, short password, unknown department, body not a JSON object) |
| `401` | Wrong email or password (the same message for both, so accounts cannot be probed) |
| `403` | Self-registration as Faculty/Admin is disabled |
| `409` | Email already registered |
| `429` | Too many failed logins for that email (5 failures lock it for 15 minutes) |
| `503` | Database unavailable |

**Planned endpoints** (the frontend already calls them): `GET /api/groups`, `GET|POST /api/groups/{id}/messages`, `GET /api/notifications`, `POST /api/announcements`, `GET /api/courses`, `GET /api/hierarchy`, `POST /api/enroll`, `POST /api/drop`, `GET /api/os/stats`, `POST /api/os/scheduler`, `POST /api/os/deadlock-test`, plus the WebSocket on `/ws`.

---

## 🔒 Security Notes

- Passwords are stored as salted **PBKDF2-HMAC-SHA256** (100,000 iterations), never in plain text.
- Login tokens are standard **HS256 JWTs** with an expiry. Tokens with a forged payload or `alg: none` are rejected.
- Every database value is sent as a **bound parameter of a prepared statement**, so user input cannot change a query.
- Registration creates the user and its student/faculty/admin profile in **one transaction**: if any step fails, nothing is saved.
- The backend uses a database account that can only read and write the `uniconnect` database.
- Request size limits and a login lockout are built in.

---

## 🧪 Tests

The build produces these test programs in `build-win\Release\`. Each ends with an `OK` line when everything passes.

| Program | What it checks | Needs MySQL |
|---|---|---|
| `stage1_test.exe` | JSON, logger, thread pool, task queue and scheduler | No |
| `stage2_test.exe` | HTTP parsing, keep-alive, pipelining, 500 concurrent requests | No |
| `stage3_test.exe` | SHA-256/HMAC/PBKDF2 against published test vectors, JWT, register/login logic, lockout | No |
| `stage3b_test.exe` | Login and register against the real database, including rollback and SQL-injection attempts | **Yes** |
| `stage4_test.exe` | The full server and the auth endpoints over real sockets | No |

```powershell
.\build-win\Release\stage1_test.exe
.\build-win\Release\stage2_test.exe
.\build-win\Release\stage3_test.exe
.\build-win\Release\stage4_test.exe
.\build-win\Release\stage3b_test.exe   # MySQL must be running with the schema loaded
```

`stage3b_test.exe` uses the development password from step 4 unless `MYSQL_PASSWORD` is set, and removes the rows it creates.

A small tool, `hashpw.exe "<password>"`, prints a password hash and the SQL to store it.

---

## 📊 Scheduler Lab

The **Scheduler lab** page shows how CPU scheduling policies behave. It works without the backend.

- Edit the processes (arrival time, burst time, priority) or paste them as JSON, or load an example (such as the *convoy effect*).
- Choose **FCFS**, **Shortest Job First**, **Priority** (higher number runs first, as in the backend scheduler) or **Round Robin** with an adjustable quantum.
- Play, pause, scrub, and change the speed of an animated Gantt chart that also shows the running process, the ready queue and the finished list.
- See waiting, turnaround and response time for every process, and a table comparing all four policies on the same input.
- A **live panel** shows the real scheduler numbers from the backend once it serves `/api/os/stats`.

The simulator does not model the backend's "aging" safeguard.

---

## 🏗️ Project Structure

```
UniConnect/
├── CMakeLists.txt             ← builds the backend, tests and tools
├── database/
│   ├── uniconnect.sql         ← schema + seed data (MySQL 8.0)
│   └── demo_passwords.sql     ← demo password for the seeded users
├── include/                   ← C++ headers
│   ├── auth/  concurrency/  database/  http/  models/  routes/  server/  services/  utils/  websocket/
├── src/                       ← C++ sources
│   ├── main.cpp               ← backend entry point
│   ├── auth/                  ← password hashing, JWT
│   ├── concurrency/           ← scheduler, task queue, thread pool
│   ├── database/              ← MySQL wrapper and user repository
│   ├── routes/                ← HTTP route handlers
│   ├── server/                ← socket event loop, connections, server wiring
│   ├── services/              ← authentication service
│   └── utils/                 ← crypto primitives
├── tests/                     ← test programs (see Tests)
├── tools/hashpw.cpp           ← password hash generator
├── scripts/run-backend.ps1    ← starts MySQL + the backend
└── frontend/                  ← React + TypeScript + Vite + Tailwind
    └── src/
        ├── api/               ← REST and WebSocket client
        ├── components/        ← layout, shared UI
        ├── pages/             ← Login, Chat, Notifications, Academics, SchedulerLab, OsDashboard
        └── sim/               ← scheduling simulation engine
```

---

## 🛠️ Tech Stack

| Layer | Technology |
|---|---|
| **Backend language** | C++20 (MSVC, Visual Studio 2022) |
| **Backend I/O** | Custom non-blocking socket event loop using Windows `WSAPoll` |
| **Database** | MySQL 8.0 through the C client library (`libmysql`), prepared statements, transactions |
| **Authentication** | JWT (HS256) and PBKDF2-SHA256, using Windows CNG for random numbers |
| **Frontend** | React 18, TypeScript, Vite, Tailwind CSS, Recharts |
| **Build** | CMake |
| **Planned** | Redis (sessions, presence), MinIO (file storage), Nginx (reverse proxy) |

---

## 📸 Architecture

```
        Browser: React + TypeScript SPA  (http://localhost:5173)
                          │  /api  and  /ws   (Vite dev proxy)
                          ▼
        C++20 backend (http://localhost:8080)
        WSAPoll event loop ─► HTTP parser ─► Router ─► Services
                                                         │
                          ┌──────────────────────────────┴───────────────┐
                          ▼                                              ▼
              MySQL 8.0 (prepared statements,              Concurrency engine
              transactions)                                task queue ─► thread pool ─► scheduler
                                                           (FCFS / SJF / RR / Priority)
```

---

## 🧯 Troubleshooting

| Problem | Fix |
|---|---|
| `cmake` or `node` "is not recognized" | Open a **new** PowerShell window, or run the PATH refresh command from step 1 |
| `MYSQL_PASSWORD is not set` | Start through `scripts\run-backend.ps1`, or set `$env:MYSQL_PASSWORD = "..."` first |
| `Cannot connect to MySQL` | MySQL is not running. Start it with the `Start-Process ... mysqld.exe` command, then retry |
| Build error: MySQL client files not found | Pass `-DMYSQL_ROOT="C:/path/to/mysql"` to the first `cmake` command |
| `libmysql.dll` or another DLL is missing when starting | Re-run `cmake --build build-win --config Release`; the build copies the DLLs next to the `.exe` |
| `Cannot bind port 8080` | Another program uses the port. Close it, or set `$env:PORT = "8081"` |
| Login says "Invalid email or password" for a demo user | `database/demo_passwords.sql` was not run |
| Login page shows a network error | The backend is not running |
| Script blocked by execution policy | Run it as `powershell -ExecutionPolicy Bypass -File .\scripts\run-backend.ps1` |
| Chat / Academics / Notifications show errors after sign-in | Expected for now. Those backend endpoints are still to be built |

---

## 🧪 Planned Test Scenarios

| Scenario | Expected Result |
|---|---|
| Student enrolls in a new course | Communication group auto-updates to include the student |
| Student drops a course | Student is automatically removed from the corresponding group(s) |
| Instructor posts an announcement | All enrolled students receive it in real time |
| Burst of concurrent chat messages | The queue processes all messages without loss or reordering |
| High concurrent load | The scheduler dispatches tasks fairly; the dashboard reflects load |
| Circular resource wait introduced | Deadlock detection identifies and reports the cycle |
| Long-waiting low-priority task | Aging prevents indefinite starvation |
| Invalid or expired JWT | Request rejected with an authentication error *(token checks are already tested)* |

---

## 🗺️ Roadmap

**Phase-I — Proposal & Design** ✅

**Phase-II — Development** *(current)*
- ✅ Database schema, HTTP server, authentication, concurrency engine, frontend shell, Scheduler lab
- 🚧 Hierarchy, courses and enrollment endpoints
- 🚧 Auto-generated groups, real-time chat (WebSocket), announcements and notifications

**Phase-III — Final Implementation**
- OS extensions: deadlock detection, aging, shared-memory telemetry
- OS live dashboard wired to the running scheduler
- Concurrent load testing, benchmarking, and evaluation
- Optional: Redis cache, MinIO file storage, Nginx

---

## 📚 References

1. Silberschatz, Galvin, Gagne — *Operating System Concepts* (scheduling, synchronization, deadlock)
2. [MySQL 8.0 documentation](https://dev.mysql.com/doc/refman/8.0/en/) (C API, prepared statements, transactions)
3. [JWT introduction](https://jwt.io/introduction)
4. [RFC 6070 — PBKDF2 test vectors](https://www.rfc-editor.org/rfc/rfc6070) and [RFC 4231 — HMAC-SHA-256 test vectors](https://www.rfc-editor.org/rfc/rfc4231)
5. [Winsock `WSAPoll`](https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-wsapoll)
6. [React](https://react.dev/), [Vite](https://vite.dev/), [Tailwind CSS](https://tailwindcss.com/docs) and [Recharts](https://recharts.org/) documentation

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

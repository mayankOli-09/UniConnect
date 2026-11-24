<div align="center">

<h1 align="center">🎓 UniConnect — Integrated University Communication & Collaboration Platform</h1>

**One hierarchy. One source of truth. Real-time communication.**

*Hierarchy-Aware Groups • Real-Time Chat • Announcements & Notifications • Concurrency Engine • CPU Scheduling • OS Live Dashboard*

[![C++](https://img.shields.io/badge/C%2B%2B-20-blue?logo=c%2B%2B&logoColor=white)](https://isocpp.org)
[![React](https://img.shields.io/badge/React-TypeScript-61DAFB?logo=react&logoColor=black)](https://react.dev)
[![MySQL](https://img.shields.io/badge/MySQL-8.0-4479A1?logo=mysql&logoColor=white)](https://www.mysql.com)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%2F11-0078D6?logo=windows&logoColor=white)](#-quick-start-windows)
[![Status](https://img.shields.io/badge/Status-Feature%20complete-brightgreen)](#-current-status)
[![License](https://img.shields.io/badge/License-MIT-lightgrey)](LICENSE)

</div>

---

## 🎯 What is UniConnect?

**UniConnect** is a real-time university communication platform in which chat rooms, announcement channels, and notification groups are **generated automatically from the university's academic hierarchy and live enrollment records**, so membership is always correct, with no manual maintenance.

> ⚠️ **The problem:** University communication today is scattered across LMS announcement boards, email listservs, and third-party chat tools (Slack, Teams, Discord). None of these automatically reflect the real organizational structure (department → program → section → course) or stay in sync when students add/drop courses. The result: missed announcements, stale groups, and constant manual admin overhead.

UniConnect treats the academic hierarchy and enrollment data as the **single source of truth** for every communication group. It is also a working showcase of core **Operating Systems** concepts (multithreading, thread pools, producer-consumer queues, synchronization, CPU scheduling, deadlock detection, shared-memory IPC) on a **C++20 backend**.

**For students and faculty it provides:**
- Automatically generated, always-accurate groups per course, section, and department
- Real-time chat over WebSockets
- Instant announcements and in-app notifications
- Role-based access for students, faculty, and admins

**Under the hood it demonstrates:**
- A custom non-blocking socket event loop (REST and WebSocket on one port)
- Work (chat messages, announcements) turned into **prioritized tasks**, ordered by a pluggable scheduler (FCFS, SJF, Round Robin, Priority) and run by a fixed-size thread pool
- A deadlock detector (wait-for graph) and shared-memory telemetry
- An interactive **Scheduler lab** that animates how each policy orders the same processes

---

## 📌 Current Status

All planned features are implemented and tested.

| Area | Status | Notes |
|---|---|---|
| MySQL schema + seed data | ✅ | `database/uniconnect.sql`, 7 seeded users |
| HTTP server (Windows, `WSAPoll` event loop) | ✅ | Keep-alive, pipelining, CORS, size limits |
| Authentication: register, login, JWT | ✅ | PBKDF2-SHA256, HS256 tokens, login lockout |
| Courses, hierarchy, enroll / drop | ✅ | Enrolling creates and fills the course, section and department groups |
| Groups and messages (REST) | ✅ | Paging with `before_id`, membership checks |
| Real-time chat (WebSocket) | ✅ | Live delivery to everyone in the group |
| Announcements and notifications | ✅ | Live push, mark as read |
| Scheduler + thread pool in the request path | ✅ | Messages and announcements run as scheduled tasks |
| OS system monitor | ✅ | Live stats, policy switching, deadlock test, load generator |
| Deadlock detector, shared-memory telemetry | ✅ | Windows named file mapping |
| Frontend: all pages, Scheduler lab | ✅ | Light and dark themes |
| Redis cache, MinIO file storage, Nginx | ➖ Not used | No part of the code needs them |
| File uploads (the `resources` table) | ➖ Not built | No endpoint in the API contract |

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

### 6. Install the frontend packages (one time)

```powershell
cd frontend
npm install
cd ..
```

### 7. Start everything

**One command** (starts MySQL, the backend and the frontend in separate windows, then opens the browser):

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\start-all.ps1
```

**Or step by step**, in two windows:

```powershell
# Window 1: MySQL (if needed) + backend
powershell -ExecutionPolicy Bypass -File .\scripts\run-backend.ps1

# Window 2: frontend
cd frontend
npm run dev
```

The backend prints `Connected to MySQL database uniconnect` and then `UniConnect backend ready`. Windows Firewall may ask for permission the first time: choose **Allow access**. Open <http://localhost:5173> and sign in with a [demo account](#-demo-accounts).

**To stop everything:** press `Ctrl+C` in the backend and frontend windows, then stop MySQL with `D:\mysql\bin\mysqladmin.exe -u root shutdown`.

A guided walk-through of every feature is in **[DEMO.md](DEMO.md)**.

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
| `PORT` | `8080` | HTTP and WebSocket port of the backend |
| `JWT_SECRET` | random per run | Token signing secret. If unset, every restart signs everyone out. **Set a long random value outside local development.** |
| `TOKEN_TTL_HOURS` | `24` | How long a login token lasts |
| `UNICONNECT_ALLOW_PRIVILEGED_SIGNUP` | off | Set to `1` to let anyone self-register as Faculty or Admin (demos only) |
| `WORKER_THREADS` | `4` | Size of the thread pool (1 to 64) |
| `SCHEDULER_POLICY` | `FCFS` | `FCFS`, `SJF`, `ROUND_ROBIN` or `PRIORITY`. Can be changed live by an admin |
| `TIME_QUANTUM_MS` | `25` | Time slice for Round Robin |
| `QUEUE_CAPACITY` | `1000` | Tasks allowed to wait for a worker; beyond this the server answers 503 "busy" |
| `UNICONNECT_LOG` | info | Set to `debug` for verbose logs |

> ⚠️ `scripts/run-backend.ps1` contains a local development password and secret. Change them, and do not reuse them anywhere real.

---

## 🔌 API Reference

All responses are JSON; errors look like `{"error": "message"}`. Every endpoint except `/health` and the two `/api/auth/*` calls needs the header `Authorization: Bearer <token>`.

| Method | Path | Who | Description |
|---|---|---|---|
| `GET` | `/health` | anyone | `{"status":"ok"}` |
| `POST` | `/api/auth/register` | anyone | Create a student account |
| `POST` | `/api/auth/login` | anyone | Sign in, returns a token |
| `GET` | `/api/courses` | signed in | All courses |
| `GET` | `/api/hierarchy` | signed in | Department → program → section → courses |
| `POST` | `/api/enroll` | student (self) / admin | `{section_id, course_id, student_id?}`; joins the course, section and department groups |
| `POST` | `/api/drop` | student (self) / admin | Same body; leaves the course group |
| `GET` | `/api/groups` | signed in | The groups you belong to |
| `GET` | `/api/groups/{id}/messages` | group members | Latest 50; `?limit=`, `?before_id=` to page back; returns `has_more` |
| `POST` | `/api/groups/{id}/messages` | group members | `{content, priority?}`; the sender is always you |
| `GET` | `/api/notifications` | signed in | Your notifications and `unread_count` |
| `POST` | `/api/announcements` | faculty (own groups) / admin (any) | `{group_id, title, content, priority?}` |
| `POST` | `/api/notifications/read` | signed in | `{ids: [...]}` or `{all: true}`; only your own |
| `GET` | `/api/os/stats` | signed in | Thread pool, scheduler, queue, deadlock, shared memory |
| `POST` | `/api/os/scheduler` | admin | `{policy, time_quantum_ms?}` |
| `POST` | `/api/os/deadlock-test` | admin | `{action: "create_cycle" \| "clear"}` |
| `POST` | `/api/os/load-test` | admin | `{count: 1..500, burst_ms: 1..200}` queues synthetic tasks |

**Register**

```http
POST /api/auth/register
{ "name": "Asha Rao", "email": "asha@example.com", "password": "Password@123", "department_id": "1" }
```

`department_id` is `1` (CSE), `2` (Mechanical) or `3` (ECE). `role` defaults to `STUDENT`; `FACULTY` and `ADMIN` return `403` unless `UNICONNECT_ALLOW_PRIVILEGED_SIGNUP=1`, so nobody can make themselves an administrator.

**Login response**

```json
{
  "success": true, "token": "eyJhbGciOi...", "token_type": "Bearer", "expires_in": 86400,
  "user": { "user_id": 1, "name": "Rahul Sharma", "email": "rahul@uniconnect.com",
            "role": "STUDENT", "department_id": 1, "student_id": 1, "section_id": 1 }
}
```

**Common status codes:** `400` invalid input · `401` not signed in or wrong credentials · `403` not allowed · `404` not found (also used for groups you are not in) · `409` already exists/enrolled · `429` too many requests · `503` database unavailable or the server is busy.

### WebSocket (`ws://localhost:8080/ws`)

JSON text frames. Client to server:

```json
{ "action": "authenticate", "token": "<login token>" }
{ "action": "subscribe", "group_id": 1 }
{ "action": "send_message", "group_id": 1, "content": "Hello", "priority": 5 }
```

Server to client:

```json
{ "event": "authenticated", "user_id": 1 }
{ "event": "subscribed", "group_id": 1 }
{ "event": "new_message", "data": { "message_id": 9, "group_id": 1, "sender_id": 1, "sender_name": "Rahul Sharma", "content": "Hello", "sent_at": "2026-10-30T10:00:00Z" } }
{ "event": "notification", "data": { "notification_id": 4, "title": "...", "content": "...", "is_read": false } }
{ "event": "error", "code": 403, "message": "..." }
```

You can only subscribe to groups you belong to, and someone removed from a group stops receiving at once even if their socket is still open.

### Limits

| What | Limit |
|---|---|
| Failed logins | 5 per email, then locked for 15 minutes |
| Messages | 30 per 10 seconds per person, 4000 characters each |
| Announcements | 10 per minute per person |
| Load tests | one per second |
| HTTP request body / WebSocket frame | 1 MB |

---

## ⚙️ How a message travels

```
Browser ──WebSocket/REST──► event loop (WSAPoll) ──► validation + permissions
                                                        │
                                                        ▼
                               Dispatcher.submit(task: priority, expected burst)
                                                        │
                                       Scheduler (FCFS / SJF / Round Robin / Priority)
                                                        │
                                                        ▼
                                          worker thread (thread pool)
                                       saves to MySQL ─► pushes to group members
```

The queue is bounded: when it is full, new work is turned away with `503` instead of piling up. The **System monitor** page shows the queue, the workers and the scheduler numbers live, and an admin can switch the policy while the server runs. Telemetry is also written to a named Windows shared-memory block (`Local\uniconnect_os_telemetry`) that any other program on the machine can read.

---

## 🔒 Security Notes

- Passwords are stored as salted **PBKDF2-HMAC-SHA256** (100,000 iterations), never in plain text.
- Login tokens are standard **HS256 JWTs** with an expiry; forged payloads and `alg: none` tokens are rejected. The WebSocket requires the same token.
- Every database value is a **bound parameter of a prepared statement**, so user input cannot change a query.
- Registration and enrollment change the user, profile and group rows in **one transaction**: if any step fails, nothing is saved.
- The sender of a message is taken from the signed-in user, never from the request.
- Group membership is checked for reading, sending and subscribing; non-members get the same answer as for a missing group.
- The backend uses a database account that can only read and write the `uniconnect` database.
- Request size limits, rate limits and a login lockout are built in.
- Not covered: HTTPS/TLS (put a reverse proxy in front for a real deployment).

---

## 🧪 Tests

The build produces these test programs in `build-win\Release\`. Each ends with an `OK` line when everything passes.

| Program | What it checks | Needs MySQL |
|---|---|---|
| `stage1_test.exe` | JSON, logger, thread pool, task queue and scheduler | No |
| `stage2_test.exe` | HTTP parsing, keep-alive, pipelining, 500 concurrent requests | No |
| `stage3_test.exe` | SHA-256/HMAC/PBKDF2 test vectors, JWT, register/login logic, lockout | No |
| `stage3b_test.exe` | Login and register against the real database, rollback, SQL-injection attempts | **Yes** |
| `stage4_test.exe` | The server and the auth endpoints over real sockets | No |
| `stage5_test.exe` | Courses, hierarchy, enroll/drop and who may do what | No |
| `stage5b_test.exe` | Enroll/drop and automatic group membership on the real database | **Yes** |
| `stage6_test.exe` | Groups and messages: permissions, validation, paging, spam limit | No |
| `stage6b_test.exe` | Groups and messages on the real seeded data | **Yes** |
| `stage7_test.exe` | WebSocket handshake, live delivery, 25 simultaneous users, scheduler pipeline | No |
| `stage8_test.exe` | Announcements, live notifications, mark as read | No |
| `stage8b_test.exe` | Announcements and notifications on the real database | **Yes** |
| `stage9a_test.exe` | Deadlock detector and shared-memory telemetry | No |
| `stage9_test.exe` | Scheduling order (FCFS/SJF/Priority), full queue, monitor endpoints, admin-only controls | No |

```powershell
Get-ChildItem .\build-win\Release\stage*_test.exe | ForEach-Object { Write-Host "== $($_.Name)"; & $_.FullName | Select-String "STAGE" }
```

The database tests use the development password from step 4 unless `MYSQL_PASSWORD` is set, and remove every row they create. A small tool, `hashpw.exe "<password>"`, prints a password hash and the SQL to store it.

---

## 📊 Scheduler Lab

The **Scheduler lab** page shows how CPU scheduling policies behave. It works without the backend.

- Edit the processes (arrival time, burst time, priority) or paste them as JSON, or load an example such as the *convoy effect*.
- Choose **FCFS**, **Shortest Job First**, **Priority** (higher number runs first, as in the backend scheduler) or **Round Robin** with an adjustable quantum.
- Play, pause, scrub, and change the speed of an animated Gantt chart that also shows the running process, the ready queue and the finished list.
- See waiting, turnaround and response time for every process, and a table comparing all four policies on the same input.
- A **live panel** shows the real scheduler numbers from the backend.

The simulator does not model the backend's "aging" safeguard.

---

## 🏗️ Project Structure

```
UniConnect/
├── CMakeLists.txt             ← builds the backend, tests and tools
├── README.md   DEMO.md
├── database/
│   ├── uniconnect.sql         ← schema + seed data (MySQL 8.0)
│   └── demo_passwords.sql     ← demo password for the seeded users
├── include/  src/             ← C++ headers and sources
│   ├── auth/                  ← password hashing, JWT
│   ├── concurrency/           ← scheduler, task queue, thread pool, dispatcher, deadlock detector, shared memory
│   ├── database/              ← MySQL wrapper and repositories (users, academics, chat, notifications)
│   ├── http/  routes/         ← HTTP parser/router and the route handlers
│   ├── server/                ← socket event loop, connections, server wiring
│   ├── services/              ← authentication, academics, chat, notifications, OS monitor
│   ├── websocket/             ← frames, handshake, live connection manager
│   └── utils/                 ← JSON, logger, crypto
├── tests/                     ← test programs (see Tests)
├── tools/hashpw.cpp           ← password hash generator
├── scripts/                   ← start-all.ps1, run-backend.ps1
└── frontend/                  ← React + TypeScript + Vite + Tailwind
    └── src/  api/  components/  pages/  sim/
```

---

## 🛠️ Tech Stack

| Layer | Technology |
|---|---|
| **Backend language** | C++20 (MSVC, Visual Studio 2022) |
| **Backend I/O** | Custom non-blocking socket event loop using Windows `WSAPoll`; REST and WebSocket on one port |
| **Concurrency** | Bounded task queue, fixed thread pool, pluggable scheduler, deadlock detector |
| **IPC** | Named Windows file mapping with a named mutex |
| **Database** | MySQL 8.0 through the C client library (`libmysql`), prepared statements, transactions |
| **Authentication** | JWT (HS256) and PBKDF2-SHA256, using Windows CNG for random numbers |
| **Frontend** | React 18, TypeScript, Vite, Tailwind CSS, Recharts |
| **Build** | CMake |

---

## 🧯 Troubleshooting

| Problem | Fix |
|---|---|
| `cmake` or `node` "is not recognized" | Open a **new** PowerShell window, or run the PATH refresh command from step 1 |
| `MYSQL_PASSWORD is not set` | Start through `scripts\run-backend.ps1`, or set `$env:MYSQL_PASSWORD = "..."` first |
| `Cannot connect to MySQL` | MySQL is not running. Start it with the `Start-Process ... mysqld.exe` command, then retry |
| Build error: MySQL client files not found | Pass `-DMYSQL_ROOT="C:/path/to/mysql"` to the first `cmake` command |
| `libmysql.dll` or another DLL is missing when starting | Re-run `cmake --build build-win --config Release`; the build copies the DLLs next to the `.exe` |
| `Cannot bind port 8080` | Another program uses the port. Close it, or set `$env:PORT = "8081"` and change the proxy in `frontend/vite.config.ts` |
| Login says "Invalid email or password" for a demo user | `database/demo_passwords.sql` was not run |
| Sidebar says "Live updates off" | The backend is not running, or was restarted. Reload the page |
| Login page shows a network error | The backend is not running |
| Script blocked by execution policy | Run it as `powershell -ExecutionPolicy Bypass -File .\scripts\run-backend.ps1` |
| Error `C2872: 'Concurrency' is ambiguous` in your own code | MSVC has a built-in `Concurrency` namespace; write `uniconnect::concurrency::` in full |

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

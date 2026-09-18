🎓 UniConnect — Integrated University Communication & Collaboration Platform
<div align="center">

One hierarchy. One source of truth. Real-time communication — done right.

Hierarchy-Aware Groups • Real-Time Chat • Announcements & Notifications • Concurrency Engine • CPU Scheduling • OS Live Dashboard

C++ React MySQL Redis MinIO Nginx Status License

</div>
🎯 What is UniConnect?

UniConnect is a real-time university communication platform in which chat rooms, announcement channels, and notification groups are generated automatically from the university's academic hierarchy and live enrollment records — so membership is always correct, with no manual maintenance.

⚠️ The problem: University communication today is scattered across LMS announcement boards, email listservs, and third-party chat tools (Slack, Teams, Discord). None of these automatically reflect the real organizational structure — university → college → department → course → section — or stay in sync when students add/drop courses. The result: missed announcements, stale groups, and constant manual admin overhead.

UniConnect solves this by treating the academic hierarchy and enrollment data as the single source of truth for every communication group, and by using the platform as a concrete, working showcase of core Operating Systems concepts — multithreading, thread pools, producer-consumer queues, synchronization, CPU scheduling, deadlock handling, and IPC — built on a production-style C++20 backend.

For Students & Faculty it provides:
Automatically generated, always-accurate chat/announcement groups per course, section, and department
Real-time chat over WebSockets
Instant announcements and push/in-app notifications
Role-based access for students, instructors, TAs, and admins
Under the hood it demonstrates:
A custom epoll-based event loop handling REST + WebSocket traffic
A bounded producer-consumer message queue feeding a fixed-size thread pool
Pluggable CPU scheduling policies (FCFS, SJF, Round Robin, Priority)
Deadlock detection, starvation/aging safeguards, and IPC via shared memory
A live OS dashboard visualizing thread activity, queue depth, and scheduler state in real time
✨ Key Features
Feature	Description
🏛️ Hierarchy Modeling	Models university → college → department → course → section as a live, queryable structure
👥 Role Management	Student, Instructor, TA, and Admin roles with JWT-based authentication
📚 Enrollment Engine	Course, section, and enrollment management backed by MySQL
🔄 Auto-Generated Groups	Communication groups regenerate automatically as hierarchy/enrollment changes — no manual list maintenance
💬 Real-Time Chat	WebSocket-based messaging between students, faculty, and groups
📢 Announcements & Notifications	Push/in-app notifications with reliable, ordered fan-out
🧵 Producer–Consumer Queue	Bounded, thread-safe message queue decoupling producers from worker threads
⚙️ CPU Scheduling	Swappable scheduling policies — FCFS, SJF, Round Robin, Priority — for task dispatch
🔒 Synchronization	Mutexes and condition variables guarding shared queues and connection state
🚦 Deadlock & Starvation Safeguards	Periodic deadlock detection (wait-for graph) and aging counters on scheduler ready queues
🔗 IPC / Shared Memory	POSIX shared memory and semaphores for cross-process worker communication
📊 OS Live Dashboard	Recharts-powered real-time view of threads, queue depth, and scheduler behavior
⚡ Caching Layer	Redis caches session state, presence, and recent messages to reduce DB load
🗂️ File Storage	MinIO handles uploaded files and attachments
🏗️ Project Structure
UniConnect/
│
├── backend/                     ← C++20 core server
│   ├── main.cpp                 # Server entry point / epoll event loop bootstrap
│   ├── event_loop/               # Custom epoll-based async I/O event loop
│   ├── queue/                   # Bounded producer-consumer message queue
│   ├── thread_pool/              # Fixed-size consumer worker thread pool
│   ├── scheduler/                # CPU scheduling policies (FCFS, SJF, RR, Priority)
│   ├── sync/                    # Mutex / condition-variable primitives
│   ├── ipc/                     # Shared memory, semaphores, deadlock detection, aging
│   ├── db/                      # MySQL access layer (libpqxx), transactions, indexing
│   ├── cache/                   # Redis integration
│   ├── storage/                 # MinIO file storage integration
│   ├── auth/                    # JWT + bcrypt authentication & role management
│   └── ws/                      # WebSocket server (chat, announcements, notifications)
│
├── frontend/                    ← React.js + TypeScript SPA
│   ├── src/
│   │   ├── components/          # UI components (chat, hierarchy, dashboard, auth)
│   │   ├── pages/                # Route-level pages
│   │   ├── dashboard/            # Recharts OS live dashboard
│   │   └── api/                  # REST + WebSocket client
│   └── package.json
│
├── infra/                       ← Deployment & infrastructure
│   ├── nginx.conf               # Reverse proxy / TLS termination config
│   └── docker-compose.yml       # MySQL, Redis, MinIO, backend, frontend services
│
└── docs/                        ← Project proposal, phase reports, diagrams

📌 The structure above reflects the planned module layout described in the Phase-I proposal; exact file names may evolve during implementation.

⚙️ Installation
Prerequisites
Linux (Ubuntu recommended) with a C++20-capable compiler (GCC 11+ or Clang 14+)
Node.js 18+ and npm/yarn (for the React + TypeScript frontend)
MySQL Server
Redis
MinIO (or a compatible S3 storage service)
Nginx (for reverse proxy / TLS termination)
VS Code (recommended IDE)
Step 1 — Clone the repository
bash
git clone https://github.com/<your-org>/uniconnect.git
cd uniconnect
Step 2 — Set up the backend (C++20)
bash
cd backend
mkdir build && cd build
cmake ..
make -j$(nproc)
Step 3 — Configure environment variables

Create a .env file with your database, cache, storage, and JWT settings:

env
MYSQL_HOST=localhost
MYSQL_USER=uniconnect
MYSQL_PASSWORD=your_password
MYSQL_DB=uniconnect_db

REDIS_HOST=localhost
REDIS_PORT=6379

MINIO_ENDPOINT=localhost:9000
MINIO_ACCESS_KEY=your_key
MINIO_SECRET_KEY=your_secret

JWT_SECRET=your_jwt_secret
Step 4 — Install frontend dependencies
bash
cd ../../frontend
npm install
🚀 Running the Project
Start supporting services (MySQL, Redis, MinIO)
bash
docker-compose -f infra/docker-compose.yml up -d
Start the backend
bash
cd backend/build
./uniconnect_server

You should see the epoll event loop come up and start listening for REST/WebSocket connections.

Keep this terminal open — the backend must stay running.

Start the frontend
bash
cd frontend
npm run dev
(Optional) Front everything with Nginx
bash
sudo nginx -c $(pwd)/infra/nginx.conf
📸 System Architecture / Workflow
                         React.js + TypeScript SPA
                        (Tailwind CSS, Recharts OS Dashboard)
                                     │
                                     ▼
                          Nginx (Reverse Proxy / TLS)
                                     │
                        HTTPS / WSS  │  REST + WebSocket
                                     ▼
                     C++20 Backend — Custom epoll Event Loop
                          REST API (JSON) + WebSocket Server
                                     │
                                     ▼
                        Producer → Bounded Message Queue
                                     │
                                     ▼
                     Thread Pool (Consumers) ── CPU Scheduler
                                                (FCFS / SJF / RR / Priority)
                                     │
                    ┌────────────────┼────────────────┐
                    ▼                ▼                 ▼
             Mutex & Cond Vars   IPC / Shared Mem   Deadlock Detection
              (Synchronization)                     + Aging (anti-starvation)
                                     │
                    ┌────────────────┼────────────────┐
                    ▼                ▼                 ▼
             MySQL (libpqxx)     Redis Cache       MinIO File Storage
           (hierarchy, users,   (sessions,        (uploads,
            enrollment, msgs)    presence)         attachments)

Data / Control Flow

The frontend sends hierarchy, enrollment, authentication, chat, and notification requests over REST/WSS.
The custom epoll event loop accepts and multiplexes both REST and WebSocket connections asynchronously.
Work items (chat messages, announcement broadcasts, notification jobs) are pushed by producers onto a bounded, thread-safe queue.
A fixed-size thread pool of consumer workers pops and processes queued items, coordinated via mutexes and condition variables and dispatched according to the active CPU scheduling policy.
MySQL persists core relational data with transactions and indexing; Redis caches hot data (sessions, presence, recent messages); MinIO stores uploaded files.
A periodic deadlock-detection routine (wait-for graph) and aging counters protect the scheduler's ready queues from deadlock and starvation.
🧩 Integration of Course Concepts
Course	Concepts Applied	Project Module
Data Structures in C	Queues, graphs, hashing, structured data organization	Communication core / scheduler
OOPs with C++	Classes, STL, modular backend design, concurrency abstractions	C++20 backend
Operating Systems	Processes, CPU scheduling, memory, synchronization, IPC, deadlock	Concurrency engine
DBMS	SQL, transactions, normalization, indexing, persistent relational data	MySQL layer
🛠️ Tech Stack
Layer	Technology
Backend Language	C++20
Backend I/O	Custom epoll-based asynchronous event loop
Frontend	React.js + TypeScript, Tailwind CSS
Database	MySQL (accessed via libpqxx), with transactions & indexing
Cache	Redis (sessions, presence, recent messages)
File Storage	MinIO
Reverse Proxy / TLS	Nginx
Authentication	JWT + bcrypt
Visualization	Recharts (OS live dashboard)
Target Environment	Linux / Ubuntu, VS Code
🧪 Test Cases (Planned)
Scenario	Expected Result
Student enrolls in a new course	Communication group auto-updates to include the student
Student drops a course	Student is automatically removed from the corresponding group(s)
Instructor posts an announcement	All enrolled students receive it in real time via WebSocket/notification
Burst of concurrent chat messages	Producer-consumer queue processes all messages without loss or reordering
High concurrent load	CPU scheduler (FCFS/SJF/RR/Priority) dispatches tasks fairly; dashboard reflects load
Circular resource wait introduced	Deadlock detection routine identifies and reports the cycle
Long-waiting low-priority task	Aging safeguard prevents indefinite starvation
Invalid/expired JWT	Request rejected with proper authentication error
🖥️ Planned Modules / Views
Module	Description
🏛️ Hierarchy & Enrollment	Manage university/college/department/course/section structure and enrollment records
💬 Chat	Real-time messaging within auto-generated course/section/department groups
📢 Announcements	Instructor/admin broadcast messages with push/in-app notification delivery
🔐 Auth & Roles	JWT-based login and role management (Student / Instructor / TA / Admin)
📊 OS Dashboard	Live view of thread pool activity, queue depth, and active scheduling policy
🗺️ Roadmap

Phase-I — Proposal & Design (current phase)

Problem motivation and current-solution analysis
University hierarchy and enrollment-based communication model
High-level system architecture and technology stack
OS concurrency engine and scheduler requirements

Phase-II — Development

Foundation: hierarchy, users, courses, enrollment
Communication core: auto-generated groups, real-time chat, announcements, notifications
Concurrency engine: thread pool, producer-consumer queue, CPU scheduler

Phase-III — Final Implementation

OS extensions: deadlock detection, starvation/aging, IPC/shared memory
OS live dashboard
Testing, concurrent load testing, benchmarking, and evaluation
Expected Outcomes
A working, hierarchy-aware university communication platform
A concurrent C++20 backend with a thread pool and producer-consumer message queue
A live OS dashboard visualizing thread activity, queue depth, and scheduler behavior
👥 Team

Team ID: OSDBMS-T109 · Team Name: MMSI · Domain: Operating Systems + DBMS Mentor: Dr. Prakash Srivastava Department of Computer Science & Engineering, Graphic Era (Deemed to be University), Dehradun — Academic Session 2026–27

Member	Role	Contribution
Mayank Oli	Team Lead	Team coordination and project design
Gara Sameera	Member	Development and documentation
Ishani Singh	Member	DBMS / testing activities
Manan Bisht	Member	Development / documentation
📚 References
Silberschatz, Galvin, Gagne — Operating System Concepts (scheduling, synchronization, deadlock, memory management)
Linux epoll(7) man page
libpqxx documentation
MySQL documentation (transactions, indexing)
Redis documentation
MinIO documentation
JWT introduction
bcrypt — Provos and Mazières, "A Future-Adaptable Password Scheme"
React and Tailwind CSS documentation
Recharts documentation
📝 Notes
This README reflects the Phase-I proposal and design of UniConnect; module names, file layout, and API details will be finalized during development (Phase-II/III).
Communication groups are derived entirely from the hierarchy and enrollment data — there is no manually maintained group list.
The concurrency engine (thread pool, scheduler, IPC, deadlock detection) is designed to be independently benchmarked and swapped, so scheduling policies can be compared under load.
Assumes a Linux/Ubuntu environment with MySQL, Redis, and MinIO available locally or via containers; enrollment/hierarchy data may be seeded or simulated for development and testing.
🤝 Contributing
bash
git checkout -b feature/your-feature-name
git commit -m "Add: your feature description"
git push origin feature/your-feature-name
# Open a Pull Request 🎉
<div align="center">

⭐ Star this repository to follow UniConnect's progress from Phase-I to final implementation!

One Hierarchy • One Platform • Real-Time, Always in Sync

Made with ❤️ using C++20 • React • MySQL • Redis • MinIO

</div>

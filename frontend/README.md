# UniConnect Frontend

Classy React + TypeScript + Tailwind frontend for the UniConnect C++ backend.

## Setup

```bash
npm install
cp .env.example .env
npm run dev
```

Open http://localhost:5173

The Vite dev server proxies `/api` to `http://localhost:8080`.

## Pages

- Login and registration
- Student dashboard
- Courses, enrollment, and course drop
- Real-time group chat via WebSocket
- Notifications and instructor announcements
- Live OS/concurrency dashboard with scheduler controls and deadlock simulation

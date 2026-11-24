# UniConnect demo script

About 10 minutes. You need two browser windows: a normal one and a private (incognito) one, so two people can be signed in at once.
Start everything first: `powershell -ExecutionPolicy Bypass -File .\scripts\start-all.ps1`. All demo accounts use the password `Password@123`.

## 1. Sign in as two people
- Window 1 (normal): http://localhost:5173, press **Student, Rahul Sharma**, then **Sign in**.
- Window 2 (private): the same address, press **Faculty, Dr. Rajesh Kumar**, then **Sign in**.

## 2. Live chat (WebSocket)
1. In both windows open **Chat** and select **DBMS - Section A**.
2. The sidebar shows "Live updates on" in both. Send a message from Rahul: it appears in Dr. Kumar's window at once, with no refresh.
3. Say: the message went through the browser's WebSocket to the server, became a prioritized task, was ordered by the scheduler, run by a worker thread, saved to MySQL and pushed to everyone in the group.

## 3. Announcement and notification
1. Dr. Kumar: **Notifications**, then "Post an announcement". Audience **DBMS - Section A**, any title and text, **Broadcast**.
2. Rahul's sidebar badge goes up by one immediately and the announcement appears at the top.
3. Rahul clicks it: it is marked read and the badge drops. Try **Mark all as read** too.

## 4. Enrollment creates and removes groups
1. Rahul: **Academics**, choose **Section A** and **CS304 Artificial Intelligence**, then **Enroll**. The message says how many groups he joined.
2. Open **Chat**: the new "Artificial Intelligence - Section A" group is there (it was generated automatically).
3. Back in Academics press **Drop**. After a refresh the course group is gone.

## 5. Scheduler lab (works without the backend)
1. Open **Scheduler lab**. The first example shows the convoy effect: one long job holds up two short ones.
2. Press **Play**. Switch between FCFS, Shortest job first, Priority and Round robin and compare the waiting times in the bottom table.
3. Say: the rules match the backend scheduler (priority: higher number first), so the lab shows what the server does.

## 6. System monitor (administrator)
1. Sign in as **Admin User** (any window), open **System monitor**.
2. Press **300 tasks** and watch the queue depth and the worker table react.
3. Change the policy to **PRIORITY**, press **Apply policy**, then **300 tasks** again and compare the waiting times.
4. **Inject circular wait** shows the deadlock detector finding a cycle (red badge and the wait-for graph). **Clear** resets it.
5. The "Shared-memory telemetry" box is the same data another program on the PC could read from the named memory block.

## 7. Permissions (optional)
- A student sees no policy or deadlock controls on the monitor.
- A student cannot post announcements. Faculty can post only to their own groups.
- Nobody can read a group they are not in, or send a message as someone else.

## Operating-systems ideas to point out
| Idea | Where it is |
|---|---|
| Producer-consumer queue | Requests become tasks waiting for a worker |
| Thread pool | Four worker threads (WORKER_THREADS) |
| CPU scheduling: FCFS, SJF, Round Robin, Priority | Switchable live; simulated in the Scheduler lab |
| Starvation and aging | Low-priority tasks gain priority while they wait |
| Mutual exclusion | Locks around the database connection, queues and shared memory |
| Deadlock detection | Wait-for graph and cycle detection on the monitor |
| Inter-process communication | Named shared memory with a mutex |
| I/O multiplexing | One thread watches all sockets with WSAPoll |

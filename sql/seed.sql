-- =======================================================================
-- UniConnect: Seed Data for Phase-I Evaluation
-- Team ID: OSDBMS-T109 (Avengers) - Graphic Era University
-- =======================================================================

-- 1. University
INSERT INTO universities (id, name, code) VALUES
('univ-geu-01', 'Graphic Era (Deemed to be University), Dehradun', 'GEU');

-- 2. College
INSERT INTO colleges (id, university_id, name, code) VALUES
('coll-soc-01', 'univ-geu-01', 'School of Computing', 'SOC');

-- 3. Department
INSERT INTO departments (id, college_id, name, code) VALUES
('dept-cse-01', 'coll-soc-01', 'Department of Computer Science & Engineering', 'CSE');

-- 4. Users (Password for all default demo users is: "password123")
-- Hash generated using salt "uniconnect_salt_2026"
INSERT INTO users (id, email, password_hash, full_name, role, department_id) VALUES
('user-mentor-01', 'prakash.srivastava@geu.ac.in', '34f0e4787a2569ba7ec118bb9d628f87b8f041ff9587a8b9eb7a3cf93fe9a805', 'Prof. Dr. Prakash Srivastava', 'INSTRUCTOR', 'dept-cse-01'),
('user-lead-01', 'oli.mayank.0904@gmail.com', '34f0e4787a2569ba7ec118bb9d628f87b8f041ff9587a8b9eb7a3cf93fe9a805', 'Mayank Oli', 'ADMIN', 'dept-cse-01'),
('user-std-01', 'garasameera918@gmail.com', '34f0e4787a2569ba7ec118bb9d628f87b8f041ff9587a8b9eb7a3cf93fe9a805', 'Gara Sameera', 'STUDENT', 'dept-cse-01'),
('user-std-02', 'ishanisingh.ncr@gmail.com', '34f0e4787a2569ba7ec118bb9d628f87b8f041ff9587a8b9eb7a3cf93fe9a805', 'Ishani Singh', 'STUDENT', 'dept-cse-01'),
('user-std-03', 'bishtmanan205@gmail.com', '34f0e4787a2569ba7ec118bb9d628f87b8f041ff9587a8b9eb7a3cf93fe9a805', 'Manan Bisht', 'STUDENT', 'dept-cse-01');

-- 5. Courses
INSERT INTO courses (id, department_id, code, title, credits, semester) VALUES
('course-os-01', 'dept-cse-01', 'CS301', 'Operating Systems Concepts & Concurrency', 4, 5),
('course-dbms-01', 'dept-cse-01', 'CS302', 'Database Management Systems & Transactions', 4, 5),
('course-ds-01', 'dept-cse-01', 'CS201', 'Data Structures & Algorithms', 3, 3);

-- 6. Sections
INSERT INTO sections (id, course_id, section_name, instructor_id, max_capacity, current_enrolled, semester, year) VALUES
('sec-os-a', 'course-os-01', 'Section A (OS)', 'user-mentor-01', 60, 3, 5, 2026),
('sec-dbms-b', 'course-dbms-01', 'Section B (DBMS)', 'user-mentor-01', 60, 3, 5, 2026);

-- 7. Enrollments
INSERT INTO enrollments (id, student_id, section_id, status) VALUES
('enr-01', 'user-std-01', 'sec-os-a', 'ENROLLED'),
('enr-02', 'user-std-02', 'sec-os-a', 'ENROLLED'),
('enr-03', 'user-std-03', 'sec-os-a', 'ENROLLED'),
('enr-04', 'user-std-01', 'sec-dbms-b', 'ENROLLED'),
('enr-05', 'user-std-02', 'sec-dbms-b', 'ENROLLED'),
('enr-06', 'user-std-03', 'sec-dbms-b', 'ENROLLED');

-- 8. Communication Groups (Hierarchy-generated)
INSERT INTO communication_groups (id, group_type, target_id, name, description) VALUES
('grp-dept-cse', 'DEPARTMENT', 'dept-cse-01', 'CSE Department General', 'All CSE faculty and enrolled students'),
('grp-sec-os-a', 'SECTION', 'sec-os-a', 'CS301 Section A Chat', 'Operating Systems course section group'),
('grp-sec-dbms-b', 'SECTION', 'sec-dbms-b', 'CS302 Section B Chat', 'Database Management Systems course section group');

-- 9. Group Memberships
INSERT INTO group_members (group_id, user_id) VALUES
('grp-dept-cse', 'user-mentor-01'),
('grp-dept-cse', 'user-lead-01'),
('grp-dept-cse', 'user-std-01'),
('grp-dept-cse', 'user-std-02'),
('grp-dept-cse', 'user-std-03'),
('grp-sec-os-a', 'user-mentor-01'),
('grp-sec-os-a', 'user-std-01'),
('grp-sec-os-a', 'user-std-02'),
('grp-sec-os-a', 'user-std-03'),
('grp-sec-dbms-b', 'user-mentor-01'),
('grp-sec-dbms-b', 'user-std-01'),
('grp-sec-dbms-b', 'user-std-02'),
('grp-sec-dbms-b', 'user-std-03');

-- 10. Sample Messages
INSERT INTO messages (id, group_id, sender_id, sender_name, content, message_type, priority) VALUES
('msg-01', 'grp-sec-os-a', 'user-mentor-01', 'Prof. Dr. Prakash Srivastava', 'Welcome to CS301 Operating Systems! We will cover CPU Scheduling, Producer-Consumer Bounded Queues, and Deadlock Detection.', 'ANNOUNCEMENT', 5),
('msg-02', 'grp-sec-os-a', 'user-lead-01', 'Mayank Oli', 'Phase-I Concurrency Engine is online with FCFS, SJF, and Round Robin schedulers.', 'TEXT', 1),
('msg-03', 'grp-sec-os-a', 'user-std-01', 'Gara Sameera', 'Confirmed, the thread pool latency is sub-millisecond on Kali Linux VM!', 'TEXT', 1),
('msg-04', 'grp-sec-dbms-b', 'user-mentor-01', 'Prof. Dr. Prakash Srivastava', 'Please review the relational normalization rules and indexing strategies for the lab evaluation.', 'ANNOUNCEMENT', 5);

-- 11. Sample Notifications
INSERT INTO notifications (id, user_id, title, message, is_read, priority) VALUES
('notif-01', 'user-std-01', 'New Announcement in CS301', 'Prof. Prakash Srivastava posted: Welcome to CS301 Operating Systems!', FALSE, 5),
('notif-02', 'user-std-02', 'New Announcement in CS301', 'Prof. Prakash Srivastava posted: Welcome to CS301 Operating Systems!', FALSE, 5),
('notif-03', 'user-std-03', 'New Announcement in CS301', 'Prof. Prakash Srivastava posted: Welcome to CS301 Operating Systems!', FALSE, 5);

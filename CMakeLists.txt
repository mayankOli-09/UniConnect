cmake_minimum_required(VERSION 3.16)

project(uniConnect
    VERSION 1.0.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

# Threading support
find_package(Threads REQUIRED)

# Common Core Sources
set(CORE_SOURCES
    src/server/Server.cpp
    src/server/EpollServer.cpp
    src/server/Connection.cpp
    src/http/HttpRequest.cpp
    src/http/HttpResponse.cpp
    src/http/HttpParser.cpp
    src/websocket/WebSocket.cpp
    src/websocket/WebSocketParser.cpp
    src/concurrency/ThreadPool.cpp
    src/concurrency/TaskQueue.cpp
    src/concurrency/Scheduler.cpp
    src/concurrency/DeadlockDetector.cpp
    src/concurrency/SharedMemoryIPC.cpp
    src/database/Database.cpp
    src/database/UserRepository.cpp
    src/database/CourseRepository.cpp
    src/database/EnrollmentRepository.cpp
    src/database/MessageRepository.cpp
    src/services/AuthService.cpp
    src/services/UserService.cpp
    src/services/CourseService.cpp
    src/services/ChatService.cpp
    src/services/NotificationService.cpp
    src/models/Models.cpp
    src/utils/Logger.cpp
    src/utils/UUID.cpp
    src/utils/Crypto.cpp
    src/utils/Json.cpp
)

# 1. Main Executable
add_executable(uniconnect
    src/main.cpp
    ${CORE_SOURCES}
)

target_include_directories(uniconnect PRIVATE include)
target_link_libraries(uniconnect PRIVATE Threads::Threads)

# Link POSIX real-time library (for shm_open on Linux)
if (UNIX AND NOT APPLE)
    target_link_libraries(uniconnect PRIVATE rt)
endif()

# 2. Unit Tests
enable_testing()

# Test Queue
add_executable(test_queue
    tests/test_queue.cpp
    src/concurrency/TaskQueue.cpp
    src/utils/Json.cpp
)
target_include_directories(test_queue PRIVATE include)
target_link_libraries(test_queue PRIVATE Threads::Threads)
add_test(NAME TestQueue COMMAND test_queue)

# Test Scheduler
add_executable(test_scheduler
    tests/test_scheduler.cpp
    src/concurrency/Scheduler.cpp
    src/concurrency/TaskQueue.cpp
    src/utils/Json.cpp
)
target_include_directories(test_scheduler PRIVATE include)
target_link_libraries(test_scheduler PRIVATE Threads::Threads)
add_test(NAME TestScheduler COMMAND test_scheduler)

# Test Database
add_executable(test_database
    tests/test_database.cpp
    src/database/Database.cpp
    src/database/UserRepository.cpp
    src/database/CourseRepository.cpp
    src/database/EnrollmentRepository.cpp
    src/database/MessageRepository.cpp
    src/models/Models.cpp
    src/utils/Crypto.cpp
    src/utils/UUID.cpp
    src/utils/Json.cpp
    src/utils/Logger.cpp
)
target_include_directories(test_database PRIVATE include)
target_link_libraries(test_database PRIVATE Threads::Threads)
add_test(NAME TestDatabase COMMAND test_database)

# Test Server & Protocols
add_executable(test_server
    tests/test_server.cpp
    src/http/HttpRequest.cpp
    src/http/HttpResponse.cpp
    src/http/HttpParser.cpp
    src/websocket/WebSocketParser.cpp
    src/websocket/WebSocket.cpp
    src/utils/Crypto.cpp
    src/utils/UUID.cpp
    src/utils/Json.cpp
    src/utils/Logger.cpp
)
target_include_directories(test_server PRIVATE include)
target_link_libraries(test_server PRIVATE Threads::Threads)
add_test(NAME TestServer COMMAND test_server)
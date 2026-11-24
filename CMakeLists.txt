cmake_minimum_required(VERSION 3.20)
project(uniConnect LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "UniConnect is a Windows-only project.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

# Shared settings for every UniConnect target
function(uniconnect_target tgt)
  target_include_directories(${tgt} PUBLIC ${CMAKE_SOURCE_DIR}/include)
  target_compile_definitions(${tgt} PUBLIC _WIN32_WINNT=0x0A00 WIN32_LEAN_AND_MEAN NOMINMAX)
  if(MSVC)
    target_compile_options(${tgt} PRIVATE /utf-8 /permissive- /W3)
  endif()
endfunction()

# ---- core library: grows with every stage ----
add_library(uniconnect_core STATIC
    src/concurrency/Scheduler.cpp
    src/concurrency/TaskQueue.cpp
    src/concurrency/ThreadPool.cpp
    src/concurrency/Dispatcher.cpp
    src/concurrency/DeadlockDetector.cpp
    src/concurrency/SharedMemoryIPC.cpp
    src/server/Connection.cpp
    src/server/EpollServer.cpp
    src/utils/Crypto.cpp
    src/auth/PasswordHasher.cpp
    src/auth/JwtService.cpp
    src/services/AuthService.cpp
    src/routes/AuthRoutes.cpp
    src/routes/AuthGuard.cpp
    src/routes/AcademicRoutes.cpp
    src/routes/ChatRoutes.cpp
    src/routes/NotificationRoutes.cpp
    src/routes/OsRoutes.cpp
    src/services/OsService.cpp
    src/services/NotificationService.cpp
    src/routes/WebSocketHandler.cpp
    src/services/ChatService.cpp
    src/services/AcademicService.cpp
    src/server/Server.cpp
)
uniconnect_target(uniconnect_core)
target_link_libraries(uniconnect_core PUBLIC Threads::Threads ws2_32 bcrypt)

# ---- stage tests ----
foreach(t stage1_test stage2_test stage3_test stage4_test stage5_test stage6_test stage7_test stage8_test stage9a_test stage9_test stage10_test stage11_test)
  add_executable(${t} tests/${t}.cpp)
  uniconnect_target(${t})
  target_link_libraries(${t} PRIVATE uniconnect_core)
endforeach()

# ---- tools ----
add_executable(hashpw tools/hashpw.cpp)
uniconnect_target(hashpw)
target_link_libraries(hashpw PRIVATE uniconnect_core)

# ---- MySQL (client library from the MySQL ZIP you unpacked) ----
set(MYSQL_ROOT "D:/mysql" CACHE PATH "Folder where the MySQL ZIP was extracted")
if(NOT EXISTS "${MYSQL_ROOT}/include/mysql.h" OR NOT EXISTS "${MYSQL_ROOT}/lib/libmysql.lib")
  message(WARNING "MySQL client files not found in ${MYSQL_ROOT}. Pass -DMYSQL_ROOT=<folder> to build the database targets.")
else()
  add_library(uniconnect_db STATIC
      src/database/MySqlDatabase.cpp
      src/database/MySqlUserRepository.cpp
      src/database/MySqlAcademicRepository.cpp
      src/database/MySqlChatRepository.cpp
      src/database/MySqlNotificationRepository.cpp
  )
  uniconnect_target(uniconnect_db)
  target_include_directories(uniconnect_db PUBLIC "${MYSQL_ROOT}/include")
  target_link_libraries(uniconnect_db PUBLIC uniconnect_core "${MYSQL_ROOT}/lib/libmysql.lib")

  # libmysql.dll (and the OpenSSL DLLs it needs) must sit next to every executable that uses it
  file(GLOB MYSQL_DLLS "${MYSQL_ROOT}/lib/*.dll" "${MYSQL_ROOT}/bin/libssl*.dll" "${MYSQL_ROOT}/bin/libcrypto*.dll")

  function(uniconnect_use_mysql tgt)
    target_link_libraries(${tgt} PRIVATE uniconnect_db)
    add_custom_command(TARGET ${tgt} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MYSQL_DLLS} $<TARGET_FILE_DIR:${tgt}>)
  endfunction()

  add_executable(stage3b_test tests/stage3b_test.cpp)
  uniconnect_target(stage3b_test)
  uniconnect_use_mysql(stage3b_test)

  add_executable(stage5b_test tests/stage5b_test.cpp)
  uniconnect_target(stage5b_test)
  uniconnect_use_mysql(stage5b_test)

  add_executable(stage6b_test tests/stage6b_test.cpp)
  uniconnect_target(stage6b_test)
  uniconnect_use_mysql(stage6b_test)

  add_executable(stage11b_test tests/stage11b_test.cpp)
  uniconnect_target(stage11b_test)
  uniconnect_use_mysql(stage11b_test)

  add_executable(stage10b_test tests/stage10b_test.cpp)
  uniconnect_target(stage10b_test)
  uniconnect_use_mysql(stage10b_test)

  add_executable(stage8b_test tests/stage8b_test.cpp)
  uniconnect_target(stage8b_test)
  uniconnect_use_mysql(stage8b_test)

  # the backend program itself
  add_executable(uniconnect src/main.cpp)
  uniconnect_target(uniconnect)
  uniconnect_use_mysql(uniconnect)
endif()

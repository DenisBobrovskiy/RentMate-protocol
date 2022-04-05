cmake_minimum_required(VERSION 3.7...3.23)

#If Cmake version <3.12, Cmake policy will be set to current version
if(${CMAKE_VERSION} VERSION_LESS 3.12)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()


project(
    MainServer VERSION 1.0
    DESCRIPTION "MainServer for the controller device of the IoT system"
    LANGUAGES C
    )



#Generate libraries

#Add arrayListLib
file(GLOB sourcesArrayList ../../misc/ArrayList/*.c) #Glob the input files
add_library(arrayListLib STATIC ${sourcesArrayList} ) #ArrayList library
set_target_properties(arrayListLib PROPERTIES LINKER_LANGUAGE C)

#Add AES-GCM lib
file(GLOB sourcesAesGcm ../../misc/AES-GCM/*.c)
add_library(aesGcmLib STATIC ${sourcesAesGcm} ) #AES-GCM helpers library
# target_link_libraries(aesGcmLib ../../misc/mbedtls/library/libmbedcrypto.a) #Add the mbedtls library
target_link_libraries(aesGcmLib /home/main/Projects/iot_system/misc/mbedtls/library/libmbedcrypto.a)
target_include_directories(aesGcmLib PUBLIC ../../misc/mbedtls/include/mbedtls) #Add the include directories for mbedtls library
set_target_properties(aesGcmLib PROPERTIES LINKER_LANGUAGE C)

#Add PckData lib
file(GLOB sourcesPckData ../../protocol/pckData/*.c)
add_library(pckDataLib STATIC ${sourcesPckData} ) #Pck data library
target_link_libraries(pckDataLib arrayListLib)
target_link_libraries(pckDataLib aesGcmLib)
target_include_directories(pckDataLib PUBLIC ../../misc/ArrayList)
target_include_directories(pckDataLib PUBLIC ../../misc/AES-GCM) 
set_target_properties(pckDataLib PROPERTIES LINKER_LANGUAGE C)

#Add tinyECDH lib
file(GLOB sourcesTinyECDH ../../misc/tinyECDH/*.c)
add_library(tinyECDHLib STATIC ${sourcesTinyECDH} ) #tinyECDH library
set_target_properties(tinyECDHLib PROPERTIES LINKER_LANGUAGE C)

#Add pthreads lib
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)



#Add executable
# file(GLOB sources *.c)
# add_executable(main ${sources})
add_executable(main server.c broadcastThread.c commands.c epollRecievingThread.c)
set_target_properties(main PROPERTIES LINKER_LANGUAGE C)


#Link libraries
target_link_libraries(main PRIVATE Threads::Threads)
target_link_libraries(main PRIVATE pckDataLib)
target_link_libraries(main PRIVATE aesGcmLib)
target_link_libraries(main PRIVATE arrayListLib)
target_link_libraries(main PRIVATE tinyECDHLib)

#Add include directories
target_include_directories(main PUBLIC ../../protocol/pckData)
target_include_directories(main PUBLIC ../../misc/AES-GCM)
target_include_directories(main PUBLIC ../../misc/ArrayList)
target_include_directories(main PUBLIC ../../misc/tinyECDH)

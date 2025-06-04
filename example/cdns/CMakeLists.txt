add_library(zmuduo_cdns resolver.cc)
target_link_libraries(zmuduo_cdns zmuduo_net cares)

add_executable(cdns dns.cc)
target_link_libraries(cdns zmuduo_cdns)


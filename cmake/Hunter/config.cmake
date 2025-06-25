hunter_config(CURL VERSION ${HUNTER_CURL_VERSION} CMAKE_ARGS HTTP_ONLY=ON CMAKE_USE_OPENSSL=ON CMAKE_USE_LIBSSH2=OFF CURL_CA_PATH=none)
hunter_config(Boost VERSION 1.72.0-p1)

# hunter_config(ethash VERSION 0.1
#     URL https://github.com/y4t3h4igb3wik/cpp-phihash/archive/refs/tags/0.1.tar.gz
#     SHA1 85aa6c856402c985922fa3ac026838b11e3e05b9
# )
hunter_config(ethash VERSION 0.1.14
    URL http://127.0.0.1:8081/cpp-phihash-master-v2.tar.gz
    SHA1 8ebc37d6bbd29d653758dd3a64ff4e07e1391df7
)

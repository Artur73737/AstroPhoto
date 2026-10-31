# IX 11.4.6 has no public receive-size limit. Build a bounded copy, leaving the
# pinned checkout untouched. Reject oversized declared frames before allocation,
# including accumulated continuation frames. Compression is disabled by our host.
set(ix_transport "${CMAKE_SOURCE_DIR}/external/ixwebsocket/ixwebsocket/IXWebSocketTransport.cpp")
file(READ "${ix_transport}" ix_source)
set(ix_anchor "            if (_rxbuf.size() < ws.header_size + ws.N)")
string(FIND "${ix_source}" "${ix_anchor}" ix_offset)
if(ix_offset EQUAL -1)
  message(FATAL_ERROR "IXWebSocket receive-size patch no longer matches")
endif()
set(ix_bound "            uint64_t accumulated = ws.N;
            for (const auto& chunk : _chunks) accumulated += chunk.size();
            if (accumulated > 1024 * 1024) {
                close(1009, \"Message exceeds the remote-control limit\");
                return;
            }
")
string(REPLACE "${ix_anchor}" "${ix_bound}${ix_anchor}" ix_source "${ix_source}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/IXWebSocketTransportBounded.cpp" "${ix_source}")
get_target_property(ix_sources ixwebsocket SOURCES)
list(FILTER ix_sources EXCLUDE REGEX "IXWebSocketTransport.cpp$")
set_property(TARGET ixwebsocket PROPERTY SOURCES "${ix_sources};${CMAKE_CURRENT_BINARY_DIR}/IXWebSocketTransportBounded.cpp")
target_include_directories(ixwebsocket PRIVATE "${CMAKE_SOURCE_DIR}/external/ixwebsocket/ixwebsocket")

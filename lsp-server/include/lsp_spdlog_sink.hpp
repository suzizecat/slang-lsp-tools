#include "spdlog/sinks/base_sink.h"
#include "lsp.hpp"

namespace slsp
{
    template<typename Mutex>
    class lsp_spdlog_sink : public spdlog::sinks::base_sink <Mutex>
    {
    public:
        inline void set_target_lsp(BaseLSP* lsp) {_lsp = lsp;};
    protected:
        BaseLSP* _lsp;
        std::string _buffer;
        void sink_it_(const spdlog::details::log_msg& msg) override
        {

        // log_msg is a struct containing the log entry info like level, timestamp, thread id etc.
        // msg.raw contains pre formatted log

        // If needed (very likely but not mandatory), the sink formats the message before sending it to its final destination:
        spdlog::memory_buf_t formatted;
        spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
        
        _buffer += fmt::to_string(formatted);

        char& end = _buffer.back();
        if(end == '\n')
            _buffer.pop_back();
        }

        void flush_() override 
        {
        _lsp->log(types::MessageType::MessageType_Log,_buffer);
        _buffer = "";
        }
    };
}
#include "spdlog/details/null_mutex.h"
#include <mutex>
namespace slsp
{
using lsp_spdlog_sink_mt = lsp_spdlog_sink<std::mutex>;
using lsp_spdlog_sink_st = lsp_spdlog_sink<spdlog::details::null_mutex>;
}
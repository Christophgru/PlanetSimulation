#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <string>

namespace rendering {
// Separate CPU wall receipts: unlike asynchronous GPU frame records, these
// include polling/collection and every early-continue or minimized-window path.
class NativeLoopProfiler {
public:
    using Clock=std::chrono::steady_clock;
    using Now=Clock::time_point (*)();
    enum class Part { EventPoll, Presentation, EventWait };
    enum class Outcome { Unpresented, Rendered, Loading, Minimized };
    explicit NativeLoopProfiler(const std::string& path="",Now now=Clock::now):now_(now) {
        if(path.empty()) return;
        output_.open(path);
        if(!output_) throw std::runtime_error("Cannot open native loop trace: "+path);
        output_ << "frame,wall_ms,event_poll_ms,presentation_ms,event_wait_ms,outcome\n" << std::setprecision(9);
    }
    NativeLoopProfiler(const NativeLoopProfiler&)=delete;
    NativeLoopProfiler& operator=(const NativeLoopProfiler&)=delete;
    bool enabled() const {return output_.is_open();}
    class Scope {
    public:
        Scope(NativeLoopProfiler& owner,std::uint64_t frame):owner_(owner.enabled() ? &owner : nullptr),frame_(frame) {
            if(owner_) {start_=owner_->now_();exceptions_=std::uncaught_exceptions();}
        }
        ~Scope() {
            if(!owner_) return;
            const double wall=milliseconds(owner_->now_()-start_);
            constexpr std::array names{"unpresented","rendered","loading","minimized"};
            owner_->output_ << frame_ << ',' << wall;
            for(double value:parts_) owner_->output_ << ',' << value;
            owner_->output_ << ',' << (std::uncaught_exceptions()>exceptions_ ? "exception" : names[static_cast<int>(outcome_)]) << '\n';
        }
        Scope(const Scope&)=delete;
        Scope& operator=(const Scope&)=delete;
        void outcome(Outcome value) {outcome_=value;}
        class Interval {
        public:
            Interval(Scope& scope,Part part):scope_(scope),part_(part) {
                if(scope_.owner_) start_=scope_.owner_->now_();
            }
            ~Interval() {
                if(scope_.owner_) scope_.parts_[static_cast<int>(part_)]+=milliseconds(scope_.owner_->now_()-start_);
            }
            Interval(const Interval&)=delete;
            Interval& operator=(const Interval&)=delete;
        private:
            Scope& scope_;
            Part part_;
            Clock::time_point start_{};
        };
    private:
        NativeLoopProfiler* owner_;
        std::uint64_t frame_;
        Clock::time_point start_{};
        std::array<double,3> parts_{};
        int exceptions_=0;
        Outcome outcome_=Outcome::Unpresented;
    };
private:
    std::ofstream output_;
    Now now_;
    static double milliseconds(Clock::duration duration) {
        return std::chrono::duration<double,std::milli>(duration).count();
    }
};
}

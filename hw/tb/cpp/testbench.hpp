#include <iostream>
#include <memory>
#include <cstring>
#include <cstdlib>
#include <verilated.h>

#if VM_TRACE
#include <verilated_fst_c.h> // Upgraded to FST
#endif


// -----------------------------------------------------------------------------
// Generic Verilator Testbench Template Class
// -----------------------------------------------------------------------------
template <class Module>
class Testbench {
    public:
        // Public variables
        uint64_t cycle_count_ = 0;
        uint64_t fail_count_  = 0;

        Testbench(int argc, char* argv[],
                  double clk_period_ns = 10.0,
                  uint64_t max_cycles = 200,
                  const char* trace_file = "waveform.fst" )
                : clk_period_ns_(clk_period_ns),
                  max_cycles_(max_cycles),
                  trace_file_(trace_file),
                  context_(std::make_unique<VerilatedContext>()) {

            context_->commandArgs(argc, argv);
            context_->randReset(2);
            context_->traceEverOn(true);

            // Instantiate whatever Verilator module is passed in
            dut_ = std::make_unique<Module>(context_.get());

            parse_plusargs();
            open_trace_if_requested();
        }

        ~Testbench() {
            dut_->final();
            #if VM_TRACE
            if (trace_) {
                trace_->close();
            }
            #endif
        }

        void reset(int cycles = 2) {
            dut_->rst_ni = 0;
            for (int i = 0; i < cycles; ++i) tick();
            dut_->rst_ni = 1;
        }

        void tick() {
            dut_->clk_i = 0;
            eval_and_dump();
            advance_time(tb_cfg::clk_period_ns_ / 2.0);
            dut_->clk_i = 1;
            eval_and_dump();
            advance_time(tb_cfg::clk_period_ns_ / 2.0);

            ++cycle_count_;
        }

        bool finished() const {
            return context_->gotFinish() || cycle_count_ >= max_cycles_;
        }

        Module* dut() { return dut_.get(); }

    private:
        std::unique_ptr<VerilatedContext> context_;
        std::unique_ptr<Module> dut_;

        #if VM_TRACE
        std::unique_ptr<VerilatedFstC> trace_;
        #endif

        double clk_period_ns_;
        uint64_t max_cycles_;
        std::string trace_file_;
        bool trace_requested_ = false;

        void parse_plusargs() {
            trace_requested_ = context_->commandArgsPlusMatch("trace") != nullptr;
            const char* max_cycles_arg = context_->commandArgsPlusMatch("max-cycles=");
            if (max_cycles_arg && std::strlen(max_cycles_arg) > 0) {
                const char* eq = std::strchr(max_cycles_arg, '=');
                if (eq) max_cycles_ = std::strtoull(eq + 1, nullptr, 10);
            }
        }

        void open_trace_if_requested() {
        #if VM_TRACE
            if (!trace_requested_) return;
            trace_ = std::make_unique<VerilatedFstC>();
            dut_->trace(trace_.get(), 99);
            trace_->open(trace_file_.c_str());
            std::cout << "[tb] waveform dump enabled -> " << trace_file_ << "\n";
        #else
            if (trace_requested_) {
                std::cerr << "[tb] WARNING: trace requested but binary built without --trace\n";
            }
        #endif
        }

        void eval_and_dump() {
            dut_->eval();
        #if VM_TRACE
            if (trace_) trace_->dump(context_->time());
        #endif
        }

        void advance_time(double delta_ns) {
            context_->timeInc(static_cast<uint64_t>(delta_ns)); // Fixed missing semicolon
        }
};

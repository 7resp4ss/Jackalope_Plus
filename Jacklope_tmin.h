#pragma once
#include <stdio.h>
#include <unistd.h>
#define TMIN_SET_MIN_SIZE   4
#define TMIN_SET_STEPS      128
#define TMIN_MAX_FILE       (100 * 1024 * 1024)
#define TRIM_MIN_BYTES      4
#define TRIM_START_STEPS    16
#define TRIM_END_STEPS      1024
class Jminimizer {
public:
    class JMinimizerContext {
        public:
          SampleDelivery *sampleDelivery;
          Instrumentation * instrumentation;

          int target_argc;
          char **target_argv;
          Coverage initial_coverage;

          ~JMinimizerContext();
        };
    JMinimizerContext *CreateJMinimizerContext(int argc, char **argv);
    void InitCoverage(JMinimizerContext *mc);
    void BinaryMinimize(JMinimizerContext *mc);
    void GrammarMinimize(JMinimizerContext *mc, std::string grammar_file);
    bool run_target(JMinimizerContext *mc, uint8_t* mem, uint32_t len);
    void ParseOptions(int argc, char **argv);
    void ReadInputFile(std::string path);
    void WriteOutputFile(std::string path, uint8_t* mem, uint32_t len);
    void ReplaceTargetCmdArg(JMinimizerContext *tc, const char *search, const char *replace);

protected:
    virtual Instrumentation *CreateInstrumentation(int argc, char **argv, JMinimizerContext *mc);
    virtual SampleDelivery* CreateSampleDelivery(int argc, char** argv, JMinimizerContext* mc);

    static uint32_t next_p2(uint32_t val) {
        uint32_t ret = 1;
        while (val > ret) ret <<= 1;
        return ret;
    
    };
    
    uint32_t timeout = 0;
    bool crash_mode = false;
    bool grammar_mini = false;
    bool no_minimize = false;
    int target_argc;
    char **target_argv;
};
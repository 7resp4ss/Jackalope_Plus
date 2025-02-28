#include "fuzzer.h"
#include "instrumentation.h"
#include "sampledelivery.h"
#include "coverage.h"
#include "directory.h"
#include "common.h"
#include "Jacklope_tmin.h"
#include <stdlib.h>
#include "mutators/grammar/grammar.h"
#include "mutators/grammar/grammarminimizer.h"

#ifndef MIN
#  define MIN(_a,_b) ((_a) > (_b) ? (_b) : (_a))
#  define MAX(_a,_b) ((_a) > (_b) ? (_a) : (_b))
#endif /* !MIN */

static std::string in_file;
static std::string out_file;
static uint32_t del_len_limit = 1; 
static uint32_t in_len, prev_len;
static uint8_t *in_data, *prev_data;
static uint32_t start_time;

uint32_t hang_times = 0;
uint32_t crash_times = 0;
bool ori_sample_is_crash = false;


void print_bytes_as_hex(unsigned char *bytes, size_t len) {
  for (size_t i = 0; i < len; i++) {
      printf("%02X ", bytes[i]);
  }
  printf("\n");
}

void Jminimizer::ReplaceTargetCmdArg(JMinimizerContext *mc, const char *search, const char *replace) {
  for (int i = 0; i < mc->target_argc; i++) {
    if (strcmp(mc->target_argv[i], search) == 0) {
      char* arg = (char*)malloc(strlen(replace) + 1);
      strcpy(arg, replace);
      mc->target_argv[i] = arg;
    }
  }
}

Instrumentation *Jminimizer::CreateInstrumentation(int argc, char **argv, JMinimizerContext *mc) {
  Instrumentation *instrumentation = NULL;

#if defined(linux) && !defined(__ANDROID__)
  char *option = GetOption("-instrumentation", argc, argv);
  if(option && !strcmp(option, "sancov")) {
    instrumentation = new SanCovInstrumentation(tc->thread_id);
  }
#endif

  if(!instrumentation) {
    instrumentation = new TinyInstInstrumentation();
  }

  instrumentation->Init(argc, argv);
  return instrumentation;
}
  
SampleDelivery *Jminimizer::CreateSampleDelivery(int argc, char **argv, JMinimizerContext *mc) {
    FileSampleDelivery* sampleDelivery = new FileSampleDelivery();
    sampleDelivery->Init(argc, argv);
    sampleDelivery->SetFilename(out_file);
    return sampleDelivery;

}

Jminimizer::JMinimizerContext *Jminimizer::CreateJMinimizerContext(int argc, char **argv) {
  JMinimizerContext *mc = new JMinimizerContext();

  // copy arguments for each thread
  mc->target_argc = target_argc;
  mc->target_argv = (char **)malloc((target_argc + 1) * sizeof(char *));
  for (int i = 0; i < target_argc; i++) {
    mc->target_argv[i] = target_argv[i];
  }
  mc->target_argv[target_argc] = NULL;

  mc->instrumentation = CreateInstrumentation(argc, argv, mc);
  mc->sampleDelivery = CreateSampleDelivery(argc, argv, mc);

  return mc;
}

Jminimizer::JMinimizerContext::~JMinimizerContext() {
  if (sampleDelivery) delete sampleDelivery;
  if (instrumentation) delete instrumentation;
  if (target_argv) free(target_argv);
}

void Jminimizer::InitCoverage(JMinimizerContext *mc) {
    Sample *sample = new Sample();
    //print_bytes_as_hex((unsigned char *)sample->bytes,sample->size);
    if (!mc->sampleDelivery->DeliverSample(sample)) {
        WARN("Error delivering sample, retrying with a clean target");
        mc->instrumentation->CleanTarget();
        bool delivery_successful = false;
        for (int retry = 0; retry < DELIVERY_RETRY_TIMES; retry++) {
          if (mc->sampleDelivery->DeliverSample(sample)) {
            WARN("Sample delivery completed successfully after %d retries\n", (retry + 1));
            delivery_successful = true;
            break;
          } else {
            WARN("Repeatedly failed to deliver sample, retrying after delay");
    #if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
            Sleep(1000);
    #else
            usleep(1000000);
    #endif
          }
        }
        if (!delivery_successful) {
          FATAL("Repeatedly failed to deliver sample");
        }
    }
    printf("InitCoverage\n");
    for(int i = 0; i < mc->target_argc; i++)
    {
      printf("mc->target_argv -> %s\n",mc->target_argv[i]);
    }
    RunResult result = mc->instrumentation->Run(mc->target_argc, mc->target_argv, timeout, timeout);
    mc->instrumentation->GetCoverage(mc->initial_coverage, true);
    if (result == CRASH) {
      printf("WTF, Crash in minimizing...\n");
      if (crash_mode)
      {
        ori_sample_is_crash = true;
        printf("crash mini mode is true\n");
      }
      else{
          crash_times = crash_times + 1;
          sample->Save(out_file.c_str());
          exit(1);
      }
  }
  delete sample;
  printf("InitCoverage Down!\n");  
  PrintCoverage(mc->initial_coverage);
}

void Jminimizer::ParseOptions(int argc, char **argv) {
    char *option;
  
    option = GetOption("-mini_in", argc, argv);
    if (!option) 
    {
      printf("Error");
      exit(0);
    }
    in_file = option;

    option = GetOption("-mini_out", argc, argv);
    if (!option) 
    {
      printf("Error");
      exit(0);
    }
    out_file = option;
    int target_opt_ind = 0;
    for (int i = 1; i < argc; i++) {
      if (strcmp(argv[i], "--") == 0) {
        target_opt_ind = i + 1;
        break;
      }
    }
  
    if (target_opt_ind) {
      target_argc = argc - target_opt_ind;
      target_argv = argv + target_opt_ind;
    } else {
      target_argc = 0;
      target_argv = NULL;
    }

    timeout = GetIntOption("-mini_t", argc, argv, 0x7FFFFFFF);
    crash_mode = GetBinaryOption("-crash_mode", argc, argv, false);
    grammar_mini = GetBinaryOption("-grammar_mini", argc, argv,false);
}

void Jminimizer::ReadInputFile(std::string path) {
    FILE *fp = fopen(path.c_str(), "rb");

    if (fp == nullptr) {
        printf("Unable to open '%s'", path.c_str());
        return;
    }

    if (fseek(fp, 0, SEEK_END)) {
        FATAL("Zero-sized input file.");
    }
    in_len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (in_len >= TMIN_MAX_FILE) {
        printf("Input file is too large (%u MB max)", TMIN_MAX_FILE / 1024 / 1024);
        fclose(fp);
        return;
    }

    in_data = (uint8_t *)malloc(in_len);
    prev_len = -1;
    prev_data = (uint8_t *)malloc(in_len);

    size_t bytes_read = fread(in_data, 1, in_len, fp);
    if (bytes_read != in_len) {
        printf("Error reading file '%s'. Only %zu bytes read.\n", path.c_str(), bytes_read);
        fclose(fp);
        return;
    }

    fclose(fp);

    printf("Read %u byte%s from '%s'.\n", in_len, in_len == 1 ? "" : "s", path.c_str());
}

void Jminimizer::WriteOutputFile(std::string path, uint8_t* mem, uint32_t len) {
    FILE *fp = fopen(path.c_str(), "wb");

    if (fp == nullptr) {
        printf("Unable to open '%s' for writing", path.c_str());
        return;
    }

    size_t bytes_written = fwrite(mem, 1, len, fp);
    if (bytes_written != len) {
        printf("Error writing to file '%s'. Only %zu byte%s written.\n", path.c_str(), bytes_written, bytes_written == 1 ? "" : "s");
        fclose(fp);
        return;
    }

    fclose(fp);

    printf("Wrote %u byte%s to '%s'.", len, len == 1 ? "" : "s", path.c_str());
}

void Jminimizer::BinaryMinimize(JMinimizerContext *mc) {
    static uint32_t alpha_map[256];

    uint8_t* tmp_buf = (uint8_t*)malloc(in_len);
    uint32_t orig_len = in_len, stage_o_len;
  
    uint32_t del_len, set_len, del_pos, set_pos, i, alpha_size, cur_pass = 0;
    uint32_t syms_removed, alpha_del0 = 0, alpha_del1, alpha_del2, alpha_d_total = 0;
    uint8_t  changed_any, prev_del;
  
    /***********************
     * BLOCK NORMALIZATION *
     ***********************/
  
    //if (no_normalize) goto next_pass;
    set_len    = next_p2(in_len / TMIN_SET_STEPS);
    set_pos    = 0;
  
    if (set_len < TMIN_SET_MIN_SIZE) set_len = TMIN_SET_MIN_SIZE;
  
    printf("Stage #0: One-time block normalization...");
  
    while (set_pos < in_len) {
  
      uint8_t  res;
      uint32_t use_len = MIN(set_len, in_len - set_pos);
  
      for (i = 0; i < use_len; i++)
        if (in_data[set_pos + i] != '0') break;
  
      if (i != use_len) {
  
        memcpy(tmp_buf, in_data, in_len);
        memset(tmp_buf + set_pos, '0', use_len);
  
        res = run_target(mc, tmp_buf, in_len);
  
        if (res) {
  
          memset(in_data + set_pos, '0', use_len);
          changed_any = 1;
          alpha_del0 += use_len;
  
        }
  
      }
  
      set_pos += set_len;
  
    }
  
    alpha_d_total += alpha_del0;
  
    printf("Block normalization complete, %u byte%s replaced.\n", alpha_del0,
        alpha_del0 == 1 ? "" : "s");
    printf("in_data -> %s\n",in_data);
  next_pass:
  
    printf("--- Pass #%u  ---\n", ++cur_pass);
    changed_any = 0;
  
    /******************
     * BLOCK DELETION *
     ******************/
  
    //if (no_minimize) goto alphabet_minimization;
    del_len = next_p2(in_len / TRIM_START_STEPS);
    stage_o_len = in_len;
  
    printf("Stage #1: Removing blocks of data...\n");
  
  next_del_blksize:
  
    if (!del_len) del_len = 1;
    del_pos  = 0;
    prev_del = 1;
  
    printf("    Block length = %u, remaining size = %u\n",
         del_len, in_len);
  
    while (del_pos < in_len) {
  
      uint8_t  res;
      int32_t tail_len;
  
      tail_len = in_len - del_pos - del_len;
      if (tail_len < 0) tail_len = 0;
  
      /* If we have processed at least one full block (initially, prev_del == 1),
         and we did so without deleting the previous one, and we aren't at the
         very end of the buffer (tail_len > 0), and the current block is the same
         as the previous one... skip this step as a no-op. */
  
      if (!prev_del && tail_len && !memcmp(in_data + del_pos - del_len,
          in_data + del_pos, del_len)) {
  
        del_pos += del_len;
        continue;
  
      }
  
      prev_del = 0;
  
      /* Head */
      memcpy(tmp_buf, in_data, del_pos);
  
      /* Tail */
      memcpy(tmp_buf + del_pos, in_data + del_pos + del_len, tail_len);
  
      res = run_target(mc, tmp_buf, in_len);
  
      if (res) {
  
        memcpy(in_data, tmp_buf, (size_t)del_pos + tail_len);
        prev_del = 1;
        in_len   = del_pos + tail_len;
  
        changed_any = 1;
  
      } else del_pos += del_len;
  
    }
  
    if (del_len > del_len_limit && in_len >= 1) {
  
      del_len /= 2;
      goto next_del_blksize;
  
    }
  
    printf("Block removal complete, %u bytes deleted.\n", stage_o_len - in_len);
  
    if (!in_len && changed_any)
        printf("Down to zero bytes - check the command line and mem limit!\n");
  
    if (cur_pass > 1 && !changed_any) goto finalize_all;
  
    /*************************
     * ALPHABET MINIMIZATION *
     *************************/
  
  alphabet_minimization:
    //if (no_normalize) goto finalize_all;
    alpha_size   = 0;
    alpha_del1   = 0;
    syms_removed = 0;
  
    memset(alpha_map, 0, 256 * sizeof(uint32_t));
  
    for (i = 0; i < in_len; i++) {
      if (!alpha_map[in_data[i]]) alpha_size++;
      alpha_map[in_data[i]]++;
    }
  
    printf("Stage #2: " "Minimizing symbols (%u code point%s)...\n", alpha_size, alpha_size == 1 ? "" : "s");
  
    for (i = 0; i < 256; i++) {
  
      uint32_t r;
      uint8_t res;
  
      if (i == '0' || !alpha_map[i]) continue;
  
      memcpy(tmp_buf, in_data, in_len);
  
      for (r = 0; r < in_len; r++)
        if (tmp_buf[r] == i) tmp_buf[r] = '0';
  
      res = run_target(mc, tmp_buf, in_len);
  
      if (res) {
  
        memcpy(in_data, tmp_buf, in_len);
        syms_removed++;
        alpha_del1 += alpha_map[i];
        changed_any = 1;
  
      }
  
    }
  
    alpha_d_total += alpha_del1;
  
    printf("Symbol minimization finished, %u symbol%s (%u byte%s) replaced.\n",
        syms_removed, syms_removed == 1 ? "" : "s",
        alpha_del1, alpha_del1 == 1 ? "" : "s");
  
    /**************************
     * CHARACTER MINIMIZATION *
     **************************/
  
    alpha_del2 = 0;
  
    printf("Stage #3: " "Character minimization...\n");
  
    memcpy(tmp_buf, in_data, in_len);
  
    for (i = 0; i < in_len; i++) {
  
      uint8_t res, orig = tmp_buf[i];
  
      if (orig == '0') continue;
      tmp_buf[i] = '0';
  
      res = run_target(mc, tmp_buf, in_len);
  
      if (res) {
  
        in_data[i] = '0';
        alpha_del2++;
        changed_any = 1;
  
      } else tmp_buf[i] = orig;
  
    }
  
    alpha_d_total += alpha_del2;
  
    printf("Character minimization done, %u byte%s replaced.\n",
        alpha_del2, alpha_del2 == 1 ? "" : "s");
  
    if (changed_any) goto next_pass;
  
  finalize_all:
  
  printf("\n"
    "      Finished minimizing : %s\n"
    "     File size reduced by : %0.02f%% (to %u byte%s)\n"
    "    Characters simplified : %0.02f%%\n"
    "     Number of execs done : %u\n"
    "             Elapsed time : %.3f secs\n\n",
    "        Crashes and Hangs : %u and %u\n\n",
    in_file.c_str(), 
    100 - ((double)in_len) * 100 / orig_len, 
    in_len, 
    in_len == 1 ? "" : "s", 
    ((double)(alpha_d_total)) * 100 / (in_len ? in_len : 1),
    (GetCurTime() - start_time) / 1000.0),
    crash_times, hang_times;
}

void Jminimizer::GrammarMinimize(JMinimizerContext *mc, std::string grammar_file) {
  Grammar grammar;
  if (!grammar.Read(grammar_file.c_str())) {
    FATAL("Error reading grammar");
  }
  Minimizer* minimizer = new GrammarMinimizer(&grammar);
  if (!minimizer) return;
  Sample *sample = new Sample();
  sample->Init((const char *)in_data, in_len);
  sample->filename = out_file.c_str();
  sample->Save();
  MinimizerContext* context = minimizer->CreateContext(sample);

  Sample test_sample = *sample;
  while (1) {
    if (!minimizer->MinimizeStep(&test_sample, context)) break;

    Coverage test_coverage;
    if (!mc->sampleDelivery->DeliverSample(sample)) {
      WARN("Error delivering sample, retrying with a clean target");
      mc->instrumentation->CleanTarget();
      bool delivery_successful = false;
      for (int retry = 0; retry < DELIVERY_RETRY_TIMES; retry++) {
        if (mc->sampleDelivery->DeliverSample(sample)) {
          WARN("Sample delivery completed successfully after %d retries\n", (retry + 1));
          delivery_successful = true;
          break;
        } else {
          WARN("Repeatedly failed to deliver sample, retrying after delay");
  #if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
          Sleep(1000);
  #else
          usleep(1000000);
  #endif
        }
      }
      if (!delivery_successful) {
        FATAL("Repeatedly failed to deliver sample");
      }
    }

    mc->instrumentation->ClearCoverage();
    mc->instrumentation->CleanTarget();
    ReplaceTargetCmdArg(mc,in_file.c_str(),out_file.c_str());
    RunResult result = mc->instrumentation->Run(mc->target_argc, mc->target_argv, timeout, timeout);
    mc->instrumentation->GetCoverage(test_coverage, true);

    if (result != OK) break;

    if (!(mc->instrumentation->GetReturnValue())
        || !CoverageContains(test_coverage, mc->initial_coverage))
    {
      minimizer->ReportFail(&test_sample, context);
      test_sample = *sample;
    } else {
      minimizer->ReportSuccess(&test_sample, context);
      *sample = test_sample;
    }
  }

  delete context;
}

bool Jminimizer::run_target(JMinimizerContext *mc, uint8_t* mem, uint32_t len) {    
    //print_bytes_as_hex(mem,len);
    // Skip run if buffer is identical to previous run
    if ((len == prev_len) && (0 == memcmp(prev_data, mem, len))) return false;
    prev_len = len;
    memcpy(prev_data, mem, len);

    Coverage new_coverage;
    Sample *sample = new Sample();
    sample->Init((const char *)mem, len);

    if (!mc->sampleDelivery->DeliverSample(sample)) {
        WARN("Error delivering sample, retrying with a clean target");
        mc->instrumentation->CleanTarget();
        bool delivery_successful = false;
        for (int retry = 0; retry < DELIVERY_RETRY_TIMES; retry++) {
          if (mc->sampleDelivery->DeliverSample(sample)) {
            WARN("Sample delivery completed successfully after %d retries\n", (retry + 1));
            delivery_successful = true;
            break;
          } else {
            WARN("Repeatedly failed to deliver sample, retrying after delay");
    #if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
            Sleep(1000);
    #else
            usleep(1000000);
    #endif
          }
        }
        if (!delivery_successful) {
          FATAL("Repeatedly failed to deliver sample");
        }
    }
    
    mc->instrumentation->ClearCoverage();
    mc->instrumentation->CleanTarget();
    ReplaceTargetCmdArg(mc,in_file.c_str(),out_file.c_str());
    RunResult result = mc->instrumentation->Run(mc->target_argc, mc->target_argv, timeout, timeout);
    mc->instrumentation->GetCoverage(new_coverage, true);
    bool ret = CoverageContains(new_coverage, mc->initial_coverage);
    /*
    printf("CoverageContains Return %d\n",ret);
    puts("initial_coverage:");
    PrintCoverage(mc->initial_coverage);
    puts("new_coverage:");
    PrintCoverage(new_coverage);
    */
    if(crash_mode && !(result == CRASH) && ret)
    {
      printf("No an Crash!!!!!!!!!\n");
      return false;
    }

    if (result == CRASH) {
        if (crash_mode && ret)
        {
          printf("Mini still crash");
          delete sample;
          return true;
        }
        else if (!crash_mode && ret){
            crash_times = crash_times + 1;
            sample->Save(out_file.c_str());
            printf("WTF Error, Crash in minimizing...");
            exit(0);
        }else{
          return false;
        }
    }else if (result == HANG)
    {
      delete sample;
      hang_times = hang_times + 1;
      return false;
    }else if (result == OK)
    {
      delete sample;
      if(ori_sample_is_crash){
        //printf("after mini sample must be a crash\n");
        return false;
      }else{
        return ret;
      }
    }
    //RunResult result = fuzzer->RunSampleAndGetCoverage(tc, sample, &initialCoverage, timeout, timeout);
}

int main(int argc, char **argv)
{
  Jminimizer* jmin = new Jminimizer();
  jmin->ParseOptions(argc, argv);   
  Jminimizer::JMinimizerContext *mc = jmin->CreateJMinimizerContext(argc, argv);
  jmin->ReadInputFile(in_file);
  jmin->InitCoverage(mc);
  //start_time = GetCurTime();
  jmin->BinaryMinimize(mc);
  printf("%s",in_data);
  return 0;
}
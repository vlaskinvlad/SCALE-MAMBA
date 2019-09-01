/*
Copyright (c) 2017, The University of Bristol, Senate House, Tyndall Avenue, Bristol, BS8 1TH, United Kingdom.
Copyright (c) 2018, COSIC-KU Leuven, Kasteelpark Arenberg 10, bus 2452, B-3001 Leuven-Heverlee, Belgium.

All rights reserved
*/

#include "Input_Output_File.h"
#include "Exceptions/Exceptions.h"

long Input_Output_File::open_channel(unsigned int channel)
{
  cout << "Player " << player_number << " Opening Channel " << channel << endl;

  // Private GFP IO
  std::string private_infile_gfp_name = datadir + "/private_input_gfp_" + std::to_string(channel) + ".txt";
  cout << "Private GFP Infile=" << private_infile_gfp_name << endl;

  std::string private_outfile_gfp_name = datadir + "/private_output_gfp_" + std::to_string(channel) + ".txt";
  cout << "Private GFP Outfile=" << private_outfile_gfp_name << endl;

  private_gfp_ifiles[channel] = new std::ifstream(private_infile_gfp_name);
  private_gfp_ofiles[channel] = new std::ofstream(private_outfile_gfp_name);

  //this snippet always sets the fail bit to zero on file open
  // so we don't fail immediately if a file doesn't exist
  auto private_gfp_state = private_gfp_ifiles[channel]->rdstate();  // get state
  private_gfp_state &= ~std::ifstream::failbit;  // remove failbit from it
  private_gfp_ifiles[channel]->clear(private_gfp_state);  // clear old state and set new state

  // these force exceptions if there is a problem with the file stream
  private_gfp_ifiles[channel]->exceptions(std::ifstream::failbit);
  private_gfp_ofiles[channel]->exceptions(std::ofstream::failbit);


  // Public GFP IO
  std::string public_infile_gfp_name = datadir + "/public_input_gfp_" + std::to_string(channel) + ".txt";
  cout << "Public GFP Infile=" << public_infile_gfp_name << endl;

  std::string public_outfile_gfp_name = datadir + "/public_output_gfp_" + std::to_string(channel) + ".txt";
  cout << "Public GFP Outfile=" << public_outfile_gfp_name << endl;

  public_gfp_ifiles[channel] = new std::ifstream(public_infile_gfp_name);
  public_gfp_ofiles[channel] = new std::ofstream(public_outfile_gfp_name);

  auto public_gfp_state = public_gfp_ifiles[channel]->rdstate();  // get state
  public_gfp_state &= ~std::ifstream::failbit;  // remove failbit from it
  public_gfp_ifiles[channel]->clear(public_gfp_state);  // clear old state and set new state

  public_gfp_ifiles[channel]->exceptions(std::ifstream::failbit);
  public_gfp_ofiles[channel]->exceptions(std::ofstream::failbit);


  // Public Int IO
  std::string public_infile_int_name = datadir + "/public_input_int_" + std::to_string(channel) + ".txt";
  cout << "Public Int Infile=" << public_infile_int_name << endl;

  std::string public_outfile_int_name = datadir + "/public_output_int_" + std::to_string(channel) + ".txt";
  cout << "Public Int Outfile=" << public_outfile_int_name << endl;

  public_int_ifiles[channel] = new std::ifstream(public_infile_int_name);
  public_int_ofiles[channel] = new std::ofstream(public_outfile_int_name);

  auto public_int_state = public_int_ifiles[channel]->rdstate();  // get state
  public_int_state &= ~std::ifstream::failbit;  // remove failbit from it
  public_int_ifiles[channel]->clear(public_int_state);  // clear old state and set new state

  public_int_ifiles[channel]->exceptions(std::ifstream::failbit);
  public_int_ofiles[channel]->exceptions(std::ofstream::failbit);

  // Share IO
  std::string share_infile_name = datadir + "/share_input_" + std::to_string(channel) + ".txt";
  cout << "Share Infile=" << share_infile_name << endl;

  std::string share_outfile_name = datadir + "/share_output_" + std::to_string(channel) + ".txt";
  cout << "Share Outfile=" << share_outfile_name << endl;

  share_ifiles[channel] = new std::ifstream(share_infile_name);
  share_ofiles[channel] = new std::ofstream(share_outfile_name);

  auto share_state = share_ifiles[channel]->rdstate();  // get state
  share_state &= ~std::ifstream::failbit;  // remove failbit from it
  share_ifiles[channel]->clear(share_state);  // clear old state and set new state

  share_ifiles[channel]->exceptions(std::ifstream::failbit);
  share_ofiles[channel]->exceptions(std::ofstream::failbit);


  return 0;
}

void Input_Output_File::close_channel(unsigned int channel)
{
  cout << "Closing Channel " << channel << endl;
  // only close files if they are open. We do this to prevent failbit
  // on files we never opened.
  if (private_gfp_ifiles[channel]->is_open()) {
    private_gfp_ifiles[channel]->close();
  }
  if (private_gfp_ofiles[channel]->is_open()) {
    private_gfp_ofiles[channel]->close();
  }

  if (public_gfp_ifiles[channel]->is_open()) {
    public_gfp_ifiles[channel]->close();
  }
  if (public_gfp_ofiles[channel]->is_open()) {
    public_gfp_ofiles[channel]->close();
  }

  if (public_int_ifiles[channel]->is_open()) {
    public_int_ifiles[channel]->close();
  }
  if (public_int_ofiles[channel]->is_open()) {
    public_int_ofiles[channel]->close();
  }

  if (share_ifiles[channel]->is_open()) {
    share_ifiles[channel]->close();
  }
  if (share_ofiles[channel]->is_open()) {
    share_ofiles[channel]->close();
  }


}

gfp Input_Output_File::private_input_gfp(unsigned int channel)
{
  word x;
  *private_gfp_ifiles[channel] >> x;
  gfp y;
  y.assign(x);
  return y;
}

void Input_Output_File::private_output_gfp(const gfp &output, unsigned int channel)
{
  *private_gfp_ofiles[channel] << output << endl;
}

gfp Input_Output_File::public_input_gfp(unsigned int channel)
{
  word x;
  *public_gfp_ifiles[channel] >> x;
  gfp y;
  y.assign(x);
  return y;

  // Important to have this call in each version of public_input_gfp
  Update_Checker(y, channel);

  return y;
}

void Input_Output_File::public_output_gfp(const gfp &output, unsigned int channel)
{
  *public_gfp_ofiles[channel] << output << endl;
}

long Input_Output_File::public_input_int(unsigned int channel)
{
  long x;
  *public_int_ifiles[channel] >> x;
  // Important to have this call in each version of public_input_gfp
  Update_Checker(x, channel);

  return x;
}

void Input_Output_File::public_output_int(const long output, unsigned int channel)
{
  *public_int_ofiles[channel] << output << endl;
}

void Input_Output_File::output_share(const Share &S, unsigned int channel)
{
  S.output(*share_ofiles[channel], human);
}

Share Input_Output_File::input_share(unsigned int channel)
{
  Share S;
  S.input(*share_ifiles[channel], human);
  return S;
}

void Input_Output_File::trigger(Schedule &schedule)
{
  printf("Restart requested: Enter a number to proceed\n");
  int i;
  cin >> i;

  // Load new schedule file program streams, using the original
  // program name
  //
  // Here you could define programatically what the new
  // programs you want to run are, by directly editing the
  // public variables in the schedule object.
  unsigned int nthreads= schedule.Load_Programs();
  if (schedule.max_n_threads() < nthreads)
    {
      throw Processor_Error("Restart requires more threads, cannot do this");
    }
}

void Input_Output_File::debug_output(const stringstream &ss)
{
  printf("%s", ss.str().c_str());
  fflush(stdout);
}

void Input_Output_File::crash(unsigned int PC, unsigned int thread_num)
{
  printf("Crashing in thread %d at PC value %d\n", thread_num, PC);
  throw crash_requested();
}

/*
Copyright (c) 2017, The University of Bristol, Senate House, Tyndall Avenue, Bristol, BS8 1TH, United Kingdom.
Copyright (c) 2019, COSIC-KU Leuven, Kasteelpark Arenberg 10, bus 2452, B-3001 Leuven-Heverlee, Belgium.

All rights reserved
*/

#include "offline_FHE.h"
#include "config.h"
#include <unistd.h>

void DistDecrypt(Plaintext &mess, const Ciphertext &ctx, const FHE_SK &sk, Player &P)
{
  const FHE_Params &params= ctx.get_params();

  vector<bigint> vv(params.phi_m());
  sk.dist_decrypt_1(vv, ctx, P.whoami());

  // Now pack into a string  for broadcasting
  vector<string> vs(P.nplayers());
  for (unsigned int i= 0; i < params.phi_m(); i++)
    {
      outputBigint(vs[P.whoami()], vv[i]);
    }

  // Broadcast and Receive the values
  P.Broadcast_Receive(vs);

  // Reconstruct the value mod p0 from all shares
  vector<bigint> vv1(params.phi_m());
  for (unsigned int i= 0; i < P.nplayers(); i++)
    {
      if (i != P.whoami())
        {
          for (unsigned int j= 0; j < params.phi_m(); j++)
            {
              inputBigint(vs[i], vv1[j]);
            }
          sk.dist_decrypt_2(vv, vv1);
        }
    }

  // Now get the final message
  bigint mod= params.p0();
  mess.set_poly_mod(vv, mod);
}

/* This one generates a new fresh ciphertext encrypting the
 * same values as the input ciphertex cm does
 *   - m is my share
 */
void Reshare(Plaintext &m, Ciphertext &cc, const Ciphertext &cm,
             Player &P, const FHE_PK &pk, const FHE_SK &sk,
             FHE_Industry &industry)
{
  Plaintext f(m.get_field());
  Ciphertext cf(cm.get_params());

  industry.Next_Off_Production_Line(f, cf, P, "Reshare");

  // We could be resharing a level 0 ciphertext so adjust if we are
  if (cm.level() == 0)
    {
      cf.Scale(m.get_field().get_prime());
    }

  // Step 4
  Ciphertext cmf(cm.get_params());
  add(cmf, cf, cm);

  // Step 5
  Plaintext mf(m.get_field());
  DistDecrypt(mf, cmf, sk, P);

  // Step 6
  if (P.whoami() == 0)
    {
      sub(m, mf, f);
    }
  else
    {
      m= f;
      m.negate();
    }

  // Step 7
  unsigned char sd[SEED_SIZE]= {0};
  PRNG G;
  G.SetSeedFromRandom(sd);
  Random_Coins rc(cm.get_params());
  rc.generate(G);
  pk.encrypt(cc, mf, rc);
  // And again
  if (cf.level() == 0)
    {
      cc.Scale(m.get_field().get_prime());
    }
  sub(cc, cc, cf);
}

/* Reshare without generating a new ciphertext */
void Reshare(Plaintext &m, const Ciphertext &cm, const Player &P,
             const FHE_SK &sk)
{
  //P.clocks[0].reset(); P.clocks[0].start();
  const FHE_Params &params= cm.get_params();

  vector<bigint> vv(params.phi_m()), ff(params.phi_m());
  sk.dist_decrypt_1a(vv, ff, cm, P.whoami());

  // Now pack into a string  for broadcasting
  if (P.whoami() != 0)
    {
      string os;
      for (unsigned int i= 0; i < params.phi_m(); i++)
        {
          outputBigint(os, vv[i]);
        }

      // Broadcast and Receive the values
      P.send_to_player(0, os);
    }
  else
    {
      // Reconstruct the value mod p0 from all shares
      vector<bigint> vv1(params.phi_m());
      for (unsigned int i= 1; i < P.nplayers(); i++)
        {
          string ss;
          P.receive_from_player(i, ss);
          for (unsigned int j= 0; j < params.phi_m(); j++)
            {
              inputBigint(ss, vv1[j]);
            }
          sk.dist_decrypt_2(vv, vv1);
        }
      for (unsigned int i= 0; i < params.phi_m(); i++)
        {
          ff[i]= ff[i] + vv[i];
        }
    }

  // Now get the final message
  m.set_poly_mod(ff, params.p0());
  //P.clocks[0].stop(); cout << "\tT0 " << P.clocks[0].elapsed() << endl;
}

/* Extracts share data from a vector */
void get_share(vector<gfp> &s, vector<gfp> &macs, const Plaintext &aa,
               const vector<Plaintext> &cc, int i)
{
  s[0]= aa.element(i);
  for (unsigned int j= 0; j < macs.size(); j++)
    {
      macs[j]= cc[j].element(i);
    }
}

void offline_FHE_triples(Player &P, list<Share> &a, list<Share> &b,
                         list<Share> &c, const FHE_PK &pk, const FHE_SK &sk,
                         const FFT_Data &PTD,
                         FHE_Industry &industry)
{
  unsigned int nmacs= P.get_mac_keys().size();

  Plaintext va(PTD), vb(PTD), vc(PTD);
  vector<Plaintext> ga(nmacs, PTD), gb(nmacs, PTD), gc(nmacs, PTD);
  Ciphertext ca(pk.get_params()), cb(pk.get_params()), cc(pk.get_params()),
      nc(pk.get_params());
  Ciphertext tmp(pk.get_params());

  while (a.size() < sz_offline_batch)
    {
      industry.Next_Off_Production_Line(va, ca, P, "Triple a");
      industry.Next_Off_Production_Line(vb, cb, P, "Triple b");

      mul(cc, ca, cb, pk);

      Reshare(vc, nc, cc, P, pk, sk, industry);

      for (unsigned int i= 0; i < nmacs; i++)
        {
          mul(tmp, ca, industry.ct_mac(i), pk);
          Reshare(ga[i], tmp, P, sk);
          mul(tmp, cb, industry.ct_mac(i), pk);
          Reshare(gb[i], tmp, P, sk);
          mul(tmp, nc, industry.ct_mac(i), pk);
          Reshare(gc[i], tmp, P, sk);
        }

      vector<gfp> s(1), macs(nmacs);
      for (unsigned int i= 0; i < pk.get_params().phi_m(); i++)
        {
          get_share(s, macs, va, ga, i);
          a.emplace_back(P.whoami(), s, macs);
          get_share(s, macs, vb, gb, i);
          b.emplace_back(P.whoami(), s, macs);
          get_share(s, macs, vc, gc, i);
          c.emplace_back(P.whoami(), s, macs);
        }
    }
}

void offline_FHE_squares(Player &P, list<Share> &a, list<Share> &b,
                         const FHE_PK &pk, const FHE_SK &sk,
                         const FFT_Data &PTD,
                         FHE_Industry &industry)
{
  unsigned int nmacs= P.get_mac_keys().size();

  Plaintext va(PTD), vc(PTD);
  vector<Plaintext> ga(nmacs, PTD), gc(nmacs, PTD);
  Ciphertext ca(pk.get_params()), cc(pk.get_params()), nc(pk.get_params());
  Ciphertext tmp(pk.get_params());

  while (a.size() < sz_offline_batch)
    {
      industry.Next_Off_Production_Line(va, ca, P, "Square a");

      mul(cc, ca, ca, pk);

      Reshare(vc, nc, cc, P, pk, sk, industry);

      for (unsigned int i= 0; i < nmacs; i++)
        {
          mul(tmp, ca, industry.ct_mac(i), pk);
          Reshare(ga[i], tmp, P, sk);
          mul(tmp, nc, industry.ct_mac(i), pk);
          Reshare(gc[i], tmp, P, sk);
        }

      vector<gfp> s(1), macs(nmacs);
      for (unsigned int i= 0; i < pk.get_params().phi_m(); i++)
        {
          get_share(s, macs, va, ga, i);
          a.emplace_back(P.whoami(), s, macs);
          get_share(s, macs, vc, gc, i);
          b.emplace_back(P.whoami(), s, macs);
        }
    }
}

void offline_FHE_bits(Player &P, list<Share> &a, const FHE_PK &pk,
                      const FHE_SK &sk, const FFT_Data &PTD,
                      FHE_Industry &industry)
{
  unsigned int nmacs= P.get_mac_keys().size();

  Plaintext va(PTD), vb(PTD);
  vector<Plaintext> ga(nmacs, PTD);
  Ciphertext ca(pk.get_params()), cb(pk.get_params());
  Ciphertext tmp(pk.get_params());

  while (a.size() < sz_offline_batch)
    { // First run the square protocol (no need to get MACs on the b=a^2 values)
      industry.Next_Off_Production_Line(va, ca, P, "Bit a");

      for (unsigned int i= 0; i < nmacs; i++)
        {
          mul(tmp, ca, industry.ct_mac(i), pk);
          Reshare(ga[i], tmp, P, sk);
        }

      mul(cb, ca, ca, pk);

      // Just decrypt cb=ca^2 now
      DistDecrypt(vb, cb, sk, P);

      // Now divide a by sqrt{b} when b<>0
      vector<gfp> s(1), macs(nmacs);
      Share ss;
      gfp a2, one= 1, twoi= 2;
      twoi.invert();
      for (unsigned int i= 0; i < pk.get_params().phi_m(); i++)
        {
          a2= vb.element(i);
          if (!a2.is_zero())
            {
              get_share(s, macs, va, ga, i);
              ss.assign(P.whoami(), s, macs);
              a2= a2.sqrRoot();
              a2.invert();
              ss.mul(ss, a2);
              ss.add(ss, one, P.get_mac_keys());
              ss.mul(ss, twoi);
              a.push_back(ss);
            }
        }
    }
}

/* We do not need to do ZKPoKs for input data, we just need for
 * the given person to encrypt some random stuff, send the
 * ciphertext, all parties multiply through by the alpha ciphertext 
 * and then reshare both ciphertexts.
 */
void offline_FHE_IO(Player &P, unsigned int player_num, list<Share> &a,
                    list<gfp> &opened, const FHE_PK &pk, const FHE_SK &sk,
                    const FFT_Data &PTD,
                    FHE_Industry &industry)
{
  // Spin until ctx_macs are ready
  while (industry.is_ready() == false)
    {
      sleep(5);
    }

  unsigned int nmacs= P.get_mac_keys().size();

  Plaintext m(PTD), va(PTD);
  vector<Plaintext> ga(nmacs, PTD);
  Ciphertext c(pk.get_params()), tmp(pk.get_params());

  // Construct the send/receive the main ciphertext
  if (P.whoami() == player_num)
    {
      m.randomize(P.G);
      c= pk.encrypt(m);
      ostringstream s;
      c.output(s);
      P.send_all(s.str());
    }
  else
    {
      string s;
      P.receive_from_player(player_num, s);
      istringstream is(s);
      c.input(is);
    }

  // Reshare the input ciphertext and the MACS
  Reshare(va, c, P, sk);
  for (unsigned int i= 0; i < nmacs; i++)
    {
      mul(tmp, c, industry.ct_mac(i), pk);
      Reshare(ga[i], tmp, P, sk);
    }

  // Construct the actual shares
  unsigned int sz= pk.get_params().phi_m();
  vector<Share> alist(sz);
  vector<gfp> openedlist(sz);
  vector<gfp> s(1), macs(nmacs);
  Share ss;
  for (unsigned int i= 0; i < sz; i++)
    {
      get_share(s, macs, va, ga, i);
      a.emplace_back(P.whoami(), s, macs);
      alist[i]= ss;
    }

  // Now extract the actual values for this player
  if (P.whoami() == player_num)
    {
      for (unsigned int i= 0; i < sz; i++)
        {
          opened.push_back(m.element(i));
        }
    }
}

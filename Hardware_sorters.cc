/*****************************************************************************[Hardware_sorters.cc]
Copyright (c) 2005-2010, Niklas Een, Niklas Sorensson

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#include "Hardware.h"

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

macro Formula operator && (Formula f, Formula g)
{
    if      (f == _0_ || g == _0_) return _0_;
    else if (f == _1_)             return g;
    else if (g == _1_)             return f;
    else if (f ==  g )             return f;
    else if (f == ~g )             return _0_;

    if (g < f) swp(f, g);
    return Bin_new(op_And, f, g);
}

macro Formula operator || (Formula f, Formula g) {
    return ~(~f && ~g); }

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -


static inline void cmp2(vec<Formula>& fs, int begin)
{
    Formula a     = fs[begin];
    Formula b     = fs[begin + 1];
#if 1
    fs[begin]     = a | b;
    fs[begin + 1] = a & b;
#else
    fs[begin]     = a || b;
    fs[begin + 1] = a && b;
#endif
}

static void riffle(vec<Formula>& fs)
{
    vec<Formula> tmp; fs.copyTo(tmp);
    for (int i = 0; i < fs.size() / 2; i++){
        fs[i*2]   = tmp[i];
        fs[i*2+1] = tmp[i+fs.size() / 2];
    }
}

static void unriffle(vec<Formula>& fs)
{
    vec<Formula> tmp; fs.copyTo(tmp);
    for (int i = 0; i < fs.size() / 2; i++){
        fs[i]               = tmp[i*2];
        fs[i+fs.size() / 2] = tmp[i*2+1];
    }
}

static void oddEvenMerge(vec<Formula>& fs, int begin, int end)
{
    assert(end - begin > 1);
    if (end - begin == 2)
        cmp2(fs,begin);
    else {
        int          mid = (end - begin) / 2;
        vec<Formula> tmp;
        for (int i = 0; i < end - begin; i++)
            tmp.push(fs[begin+i]);
        unriffle(tmp);
        oddEvenMerge(tmp,0,mid);
        oddEvenMerge(tmp,mid,tmp.size());
        riffle(tmp);
        for (int i = 1; i < tmp.size() - 1; i += 2)
            cmp2(tmp,i);
        for (int i = 0; i < tmp.size(); i++)
            fs[i + begin] = tmp[i];
    }
}

// Inputs to the circuit is the formulas in fs, which is overwritten
// by the resulting outputs of the circuit.
// NOTE: The number of comparisons is bounded by: n * log n * (log n + 1)
static void oldOddEvenSort(vec<Formula>& fs)
{
    int orig_sz = fs.size();
    int sz; for (sz = 1; sz < fs.size(); sz *= 2);
    fs.growTo(sz,_0_);

    for (int i = 1; i < fs.size(); i *= 2)
        for (int j = 0; j + 2*i <= fs.size(); j += 2*i)
            oddEvenMerge(fs,j,j+2*i);
    fs.shrink(sz - orig_sz);
}


void oddEvenSort(vec<Formula>& invars, int max_sel, int ineq);

static inline bool preferDirectMerge(unsigned n, unsigned k);
static inline void sort2(vec<Formula>& vars, unsigned i, unsigned j); // comparator

static void DirectCardClausesLT(const vec<Formula>& invars, unsigned start, 
                     unsigned pos, unsigned j, vec<Formula>& args);
static void DirectCardClausesGT(const vec<Formula>& invars, unsigned start, 
                     unsigned pos, unsigned j, vec<Formula>& args);

static void DirectSortLT(const vec<Formula>& invars, vec<Formula>& outvars, unsigned k);
static void DirectSortGT(const vec<Formula>& invars, vec<Formula>& outvars, unsigned k);

static void DirectMergeLT(const vec<Formula>& in1, const vec<Formula>& in2, 
                    vec<Formula>& outvars, unsigned k);
static void DirectMergeGT(const vec<Formula>& in1, const vec<Formula>& in2, 
                    vec<Formula>& outvars, unsigned k);

static void OddEvenSelect(const vec<Formula>& invars, vec<Formula>& outvars, 
                    unsigned k,int ineq);
static void OddEvenCombine(const vec<Formula>& in1, const vec<Formula>& in2, 
                    vec<Formula>& outvars, unsigned k);
static void OddEvenMerge(const vec<Formula>& in1,  const vec<Formula>& in2, 
                    vec<Formula>& outvars, unsigned k);

static void OddEven4Select(const vec<Formula>& invars, vec<Formula>& outvars, 
                    unsigned k, int ineq);
static void OddEven4Combine(vec<Formula> const& x, vec<Formula> const& y, 
                    vec<Formula>& outvars, unsigned k);
static void OddEven4Merge(vec<Formula> const& a, vec<Formula> const& b, vec<Formula> const& c, 
                    vec<Formula> const& d, vec<Formula>& outvars, unsigned int k, int ineq);

void oddEvenSort(vec<Formula>& invars, int max_sel, int ineq)
{
    vec<Formula> outvars;
    switch (opt_sort_alg) {
        case 1:
            oldOddEvenSort(invars);
            return;
        case 2:
            OddEvenSelect(invars,outvars,max_sel < 0 || max_sel >= invars.size() ? invars.size() : max_sel, ineq);
            break;
         case 4:
            OddEven4Select(invars,outvars,max_sel < 0 || max_sel >= invars.size() ? invars.size() : max_sel, ineq);
            break;
    }  
    outvars.copyTo(invars);
}

static inline unsigned pow2roundup (unsigned x) {
    if(x == 0) return 0;
    --x;
    x |= x >> 1;
    x |= x >> 2;
    x |= x >> 4;
    x |= x >> 8;
    x |= x >> 16;
    return x+1;
}

static inline bool preferDirectMerge(unsigned n, unsigned k) {
    static unsigned minTest = 94, maxTest = 201;
    static unsigned short nBound[] = {
#include "DirOrOddEvenMerge.inl"
  } ;
  if (k < minTest) return true;
  if (k > maxTest) return false;
  return n < nBound[k-minTest];
}

static inline void sort2(vec<Formula>& vars, unsigned i, unsigned j) // comparator
{
    Formula a = vars[i], b = vars[j];
    vars[i] = a | b;
    vars[j] = a & b;
}

static void DirectCardClausesLT(const vec<Formula>& invars, unsigned start, unsigned pos, unsigned j, vec<Formula>& args) {
  // 1s will be propagated from inputs to outputs.
    unsigned n = invars.size();
  if (pos == j) {
    Formula conj = _1_;
    for (unsigned i=0 ; i < j ; i++) conj = conj && args[i];
    args[j] = args[j] || conj;
  } else {
    for (unsigned i = start ; i <= n - (j - pos) ; i++) {
      args[pos] = invars[i];
      DirectCardClausesLT(invars, i+1, pos+1, j, args);
    }
  }  
}

static void DirectCardClausesGT(const vec<Formula>& invars, unsigned start, unsigned pos, unsigned j, vec<Formula>& args) {
  // 0s will be propagated from inputs to outputs.
  unsigned n = invars.size();
  if (pos == j) {
    Formula disj = _0_;
    for (unsigned i=0 ; i < j ; i++) disj = disj || args[i];
    args[j] = args[j] && disj;
  } else {
    for (unsigned i = start ; i <= n - (j - pos) ; i++) {
      args[pos] = invars[i];
      DirectCardClausesGT(invars, i+1, pos+1, j, args);
    }
  }  
}

static void DirectSortLT(const vec<Formula>& invars, vec<Formula>& outvars, unsigned k) {
  // as described in CP'2013 paper: Abio, Nieuwenhuis, Oliveras and Rodriguez-Carbonell:
  // "A Parametric Approach for Smaller and Better Encodings of Cardinality Constraints", page 11
  // k is the desired size of sorted output; 1s will be propagated from inputs to outputs.

  // outvars should be created in this function
  assert(outvars.empty());
  unsigned n = invars.size();
  
  if (k > n) k = n;

  for (unsigned j=1 ; j <= k ; j++) {
    vec<Formula> args;
    for (unsigned i=0 ; i < j; i++) args.push(_1_);
    args.push(_0_);
    DirectCardClausesLT(invars, 0, 0, j, args); // set outvars[j-1] to \/ (1<= i1 < .. <ij <= n) /\ (s=1..j) invars[is]
    outvars.push(args[j]);
  }
  return;
}

static void DirectSortGT(const vec<Formula>& invars, vec<Formula>& outvars, unsigned k) {
  // as described in CP'2013 paper: Abio, Nieuwenhuis, Oliveras and Rodriguez-Carbonell:
  // "A Parametric Approach for Smaller and Better Encodings of Cardinality Constraints", page 11
  // k is the desired size of sorted output; 0s will be propagated from inputs to outputs.

  // outvars should be created in this function
  assert(outvars.empty());
  unsigned n = invars.size();
  
  if (k > n) k = n;

  for (unsigned j=n ; j > n-k ; j--) {
    vec<Formula> args;
    for (unsigned i=0 ; i < j; i++) args.push(_0_);
    args.push(_1_);
    DirectCardClausesGT(invars, 0, 0, j, args); // set outvars[j-1] to /\ (1<= i1 < .. <ij <= n) \/ (s=1..j) invars[is]
    outvars.push(args[j]);
  }
  return;
}

static void DirectMergeLT(const vec<Formula>& in1, const vec<Formula>& in2,vec<Formula>& outvars, unsigned k) {
  // outvars should be created in this function
  // k is the desired size of sorted output; 1s will be propagated from inputs to outputs.
  assert(outvars.empty());
  
  unsigned a = in1.size(), b = in2.size(), c = min(a+b,k);
  if (a > c) a = c;
  if (b > c) b = c;

  if (b == 0)
    for (unsigned i=0 ; i < c ; i++) outvars.push(in1[i]);
  else if (a == 0)
    for (unsigned i=0 ; i < c ; i++) outvars.push(in2[i]);
  else {
    for (unsigned i=0 ; i < c ; i++) outvars.push(_0_);
    for (unsigned i=0 ; i < a ; i++) outvars[i] |= in1[i];
    for (unsigned i=0 ; i < b ; i++) outvars[i] |= in2[i];
    for (unsigned j=0 ; j < b ; j++)
      for(unsigned i=0 ; i < min(a,c-j-1) ; i++) outvars[i+j+1] |= in1[i] & in2[j];
  }
}

static void DirectMergeGT(const vec<Formula>& in1, const vec<Formula>& in2,vec<Formula>& outvars, unsigned k) {
  // outvars should be created in this function
  // k is the desired size of sorted output; 0s will be propagated from inputs to outputs.
  assert(outvars.empty());
  
  unsigned n = in1.size(), m = in2.size(), c = min(n+m,k), a = min(n,c), b = min(m,c);

  if (b == 0)
    for (unsigned i=0 ; i < c ; i++) outvars.push(in1[i]);
  else if (a == 0)
    for (unsigned i=0 ; i < c ; i++) outvars.push(in2[i]);
  else {
    for (unsigned i=0 ; i < c ; i++) outvars.push(_1_);
    for (unsigned i=0 ; i < min(a,c-m) ; i++) outvars[i+m] &= in1[i];
    for (unsigned i=0 ; i < min(b,c-n) ; i++) outvars[i+n] &= in2[i];
    for (unsigned j=0 ; j < b ; j++)
      for(unsigned i=0 ; i < min(a,c-j) ; i++) outvars[i+j] &= in1[i] | in2[j];
  }
}

static void OddEvenSelect(const vec<Formula>& invars, vec<Formula>& outvars, unsigned k, int ineq) {
  assert(outvars.empty());

  unsigned n = invars.size();

  assert(k <= n);

  if (n == 0 || k == 0) return;
  if (n == 1) {
    outvars.push(invars[0]);
    return;
  }
  if (ineq < 0 && (k==1 || k==2 && n <= 39 || k==3 && n <= 10 || k==4 && n <= 8 || n<=6)) {
       DirectSortLT(invars, outvars, k);
       return;
  }
  if (ineq > 0 && (k==1 || k==2 && n <= 11 || k==3 && n <= 9 || k==4 && n <= 7 || n<=6)) {
      DirectSortGT(invars, outvars, k);
      return;
  }

  unsigned n1, n2;
  unsigned p2 = (k==2 ? 1 : pow2roundup((k+4)/3));
  n2 = (n <= 7 ? n/2 : abs((int)k/2-p2) > (k+3)/4 ? (k <= n/2 ? k : k/2) : (2*p2 <= n/2 ? 2*p2 : p2) );
  n1 = n - n2;

  // split
  vec<Formula> x, y;
  for (unsigned i=0; i < n1; i++) x.push(invars[i]);
  for (unsigned i=n1; i < n; i++) y.push(invars[i]);

  // recursive calls
  vec<Formula> _x, _y;
  OddEvenSelect(x, _x, min(k, n1), ineq);
  OddEvenSelect(y, _y, min(k, n2), ineq);

  // merging
  if (ineq != 0 && preferDirectMerge(n,k))
      if (ineq < 0) DirectMergeLT(_x,_y,outvars,k);
      else          DirectMergeGT(_x,_y,outvars,k);
  else
      OddEvenMerge(_x, _y, outvars,k);

  return;
}


static void OddEvenCombine(const vec<Formula>& in1, const vec<Formula>& in2, vec<Formula>& outvars, unsigned k) {
    unsigned a = in1.size(), b = in2.size();
    if (k > a+b) k = a+b;   
 
    // set outvars[0] = in1[0];
    outvars.push(in1[0]);

    for (unsigned i = 0 ; i < (k-1)/2 ; i++) {
        outvars.push(in2[i]); outvars.push(in1[i+1]);
        sort2(outvars, i*2+1, i*2+2);
    }

    // set outvars[k-1] if k is even
    if (k % 2 == 0)
        if (k < a + b)   outvars.push(in1[k/2] | in2[k/2-1]);
        else if (a == b) outvars.push(in2[k/2-1]);
        else             outvars.push(in1[k/2]);
}
    
static void OddEvenMerge(const vec<Formula>& in1, const vec<Formula>& in2, vec<Formula>& outvars, unsigned k) {
    unsigned a = in1.size(), b = in2.size();
    
    if (a+b < k) k = a+b;
    if (a > k) a = k;
    if (b > k) b = k;
    if (a == 0) {
        for (unsigned i = 0 ; i < b ; i++) outvars.push(in2[i]);
	return;
    }
    if (b == 0) {
        for (unsigned i = 0 ; i < a ; i++) outvars.push(in1[i]);
	return;
    }
    if (a == 1 && b == 1) {
      if (k == 1) outvars.push(in1[0] | in2[0]);
      else { // k == 2
        outvars.push(in1[0]); outvars.push(in2[0]);
        sort2(outvars, 0, 1);
      }
      return;
    }
    // from now on: a > 0 && b > 0 && && a,b <= k && 1 < k <= a + b 
    
    vec<Formula> in1odds, in2odds, in1evens, in2evens, tmp1, tmp2;
    // in1evens = in1[0,2,4,...], in2evens same
    // in1odds  = in1[1,3,5,...], in2odds same
    for (unsigned i = 0 ; i < a; i+=2) {
        in1evens.push(in1[i]);
        if (i + 1 < a) in1odds.push(in1[i+1]);
    }
    for (unsigned i = 0 ; i < b; i+=2) {
        in2evens.push(in2[i]);
        if (i + 1 < b) in2odds.push(in2[i+1]);
    }
    OddEvenMerge(in1evens, in2evens, tmp1, k/2+1);
    OddEvenMerge(in1odds, in2odds, tmp2, k/2);
    OddEvenCombine(tmp1,tmp2,outvars,k);
}

static void OddEven4Select(const vec<Formula>& invars, vec<Formula>& outvars, 
                        unsigned k, int ineq) {
  assert(outvars.empty());

  unsigned n = invars.size();

  assert(k <= n);

  if (n==0 || k == 0) return;
  if (n == 1) {
    outvars.push(invars[0]);
    return;
  }
 
  if (ineq < 0 && (k == 1 || k == 2 && n <= 8 || n <= 6)) {
      DirectSortLT(invars, outvars, k);
      return;
  } else if (ineq > 0 && (k == 1 || k == 2 && n <= 7 || n <= 5)) {
      DirectSortGT(invars, outvars, k);
      return;
  }
  
  // select sizes
  unsigned n1, n2, n3, n4;
  if (n <= 7) { 
    n4=n/4; n3=(n+1)/4; n2=(n+2)/4; 
  } else {
    int p2 = (k/2==2 ? 1 : pow2roundup((k/2+4)/3));
    n4 = (abs((int)k/4-p2) > ((int)k/2+3)/4 || 4*p2 > (int)n ? k/4 : p2);
    n3 = (n == k && n4 == k/4 ? (k+1)/4 : n4);
    n2 = (n == k && n4 == k/4 ? (k+2)/4 : n4);
  }
  n1 = n - n2 - n3 - n4;
  
  // split
  vec<Formula> a, b, c, d;
  for (unsigned i=0; i < n1; i++) a.push(invars[i]);
  for (unsigned i=0; i < n2; i++) b.push(invars[n1+i]);
  for (unsigned i=0; i < n3; i++) c.push(invars[n1+n2+i]);
  for (unsigned i=0; i < n4; i++) d.push(invars[n1+n2+n3+i]);
  
  // recursive calls
  unsigned k1 = min(k, n1), k2 = min(k, n2), k3 = min(k, n3), k4 = min(k, n4);
  vec<Formula> _a, _b, _c, _d;
  OddEven4Select(a, _a, k1,ineq);
  OddEven4Select(b, _b, k2,ineq);
  OddEven4Select(c, _c, k3,ineq);
  OddEven4Select(d, _d, k4,ineq);

  // merging
  if (ineq != 0 && preferDirectMerge(n,k)) {
      vec<Formula> out1,out2;
      if (ineq < 0) {
         DirectMergeLT(_a,_b,out1,min(k1+k2,k));
         DirectMergeLT(_c,_d,out2,min(k3+k4,k));
         DirectMergeLT(out1,out2,outvars,k);
      } else {
         DirectMergeGT(_a,_b,out1,min(k1+k2,k));
         DirectMergeGT(_c,_d,out2,min(k3+k4,k));
         DirectMergeGT(out1,out2,outvars,k);
      }
  } else {
      OddEven4Merge(_a, _b, _c, _d, outvars, k,ineq);
    //  vec<Formula> out1,out2;
    //  OddEvenMerge(_a,_b,out1,min(k1+k2,k));
    //  OddEvenMerge(_c,_d,out2,min(k3+k4,k));
    //  OddEvenMerge(out1,out2,outvars,k);

  } 
}

static void OddEven4Combine(vec<Formula> const& x, vec<Formula> const& y, 
                           vec<Formula>& outvars, unsigned k) {
    unsigned a = x.size(), b = y.size();
    assert(a >= b); assert(a <= b+4); assert(a >= 2); assert(b >= 1); 
    if (k > a+b) k = a+b;   

    // set outvars[0] = x[0];
    outvars.push(x[0]);
    unsigned last = (k < a+b || k % 2 == 1 || a == b+2 ? k : k-1);
    for (unsigned i = 0, j = 1 ; j < last ; j++,i=j/2) {
	Formula ret = _0_;
        if (j %2 == 0) {
	    if (i+1 < a && i < b+2) ret = ret || x[i+1] && (i >= 2 ? y[i-2] : _1_);
            if (i < a && i < b+1)   ret = ret || x[i] && y[i-1];
        } else {
            if (i > 0 && i+2 < a)   ret = ret || x[i+2];
            if (i < b)              ret = ret || y[i];
            if (i+1 < a && i < b+1) ret = ret || x[i+1] && (i >= 1 ? y[i-1] : _1_);
        }
        outvars.push(ret);

    }
    if (k == a+b && k % 2 == 0 && a != b+2)
        if (a == b) outvars.push(y[b-1]);
        else outvars.push(x[a-1]);
}
    
static void OddEven4Merge(vec<Formula> const& a, vec<Formula> const& b, vec<Formula> const& c, 
                    vec<Formula> const& d, vec<Formula>& outvars, unsigned int k, int ineq) {
    unsigned na = a.size(), nb = b.size(), nc = c.size(), nd = d.size();

    assert(na > 0); assert(na >= nb); assert(nb >= nc); assert(nc >= nd);
    if (na+nb+nc+nd < k) k = na+nb+nc+nd;
    if (na > k) na = k;
    if (nb > k) nb = k;
    if (nc > k) nc = k;
    if (nd > k) nd = k;
    
    if (nb == 0) {
        for (unsigned i = 0 ; i < na ; i++) outvars.push(a[i]);
	return;
    }
    if (na == 1) {
        vec<Formula> invars;
        invars.push(a[0]); invars.push(b[0]);
        if (nc > 0) invars.push(c[0]);
        if (nd > 0) invars.push(d[0]);
        if (ineq < 0)      DirectSortLT(invars, outvars, k); 
        else if (ineq > 0) DirectSortGT(invars, outvars, k);
        else               OddEvenSelect(invars, outvars, k, ineq);
        return;
    }
    // from now on: na > 1 && nb > 0 
    vec<Formula> aodds, aevens, bodds, bevens, codds, cevens, dodds, devens, x, y;
    // split into odds and evens
    for (unsigned i = 0 ; i < na; i+=2) {
        aevens.push(a[i]);
        if (i + 1 < na) aodds.push(a[i+1]);
    }
    for (unsigned i = 0 ; i < nb; i+=2) {
        bevens.push(b[i]);
        if (i + 1 < nb) bodds.push(b[i+1]);
    }
    for (unsigned i = 0 ; i < nc; i+=2) {
        cevens.push(c[i]);
        if (i + 1 < nc) codds.push(c[i+1]);
    }
    for (unsigned i = 0 ; i < nd; i+=2) {
        devens.push(d[i]);
        if (i + 1 < nd) dodds.push(d[i+1]);
    }
    
    // recursive merges
    OddEven4Merge(aevens, bevens, cevens, devens, x, k/2+2,ineq);
    OddEven4Merge(aodds, bodds, codds, dodds, y, k/2,ineq);
    
    // combine results
    OddEven4Combine(x,y,outvars,k);
}
/*****************************************************************************[PbSolver_convert.cc]
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

#include "PbSolver.h"
#include "Hardware.h"

//-------------------------------------------------------------------------------------------------
void    linearAddition (const Linear& c, vec<Formula>& out);        // From: PbSolver_convertAdd.C
Formula buildConstraint(const Linear& c, int max_cost = INT_MAX);   // From: PbSolver_convertSort.C
Formula convertToBdd   (const Linear& c, int max_cost = INT_MAX);   // From: PbSolver_convertBdd.C
//-------------------------------------------------------------------------------------------------


bool PbSolver::convertPbs(bool first_call)
{
    vec<Formula>    converted_constrs;
    ConvertT saved_opt_convert = opt_convert;

    if (first_call){
        findIntervals();
        if (!rewriteAlmostClauses()) {
            sat_solver.addEmptyClause();
            return false;
        }
    }

    for (int i = 0; i < constrs.size(); i++) {
        if (constrs[i] == NULL) continue;
        Linear& c   = *constrs[i]; assert(c.lo != Int_MIN || c.hi != Int_MAX);

        if (opt_verbosity >= 1 && first_call) /**/ reportf("---[%4d]---> ", constrs.size() - 1 - i);
    
        try { // M. Piotrow 11.10.2017
            if (opt_convert == ct_Sorters)
                converted_constrs.push(buildConstraint(c)), first_call ? ++srtEncodings : ++srtOptEncodings;
            else if (opt_convert == ct_Adders)
                linearAddition(c, converted_constrs), first_call ? ++addEncodings : ++addOptEncodings;
            else if (opt_convert == ct_BDDs)
                converted_constrs.push(convertToBdd(c)), first_call ? ++bddEncodings : ++bddOptEncodings;
            else if (opt_convert == ct_Mixed) {
                int adder_cost = estimatedAdderCost(c);
                //int different_cs = 1; for (int i = 1; i < c.size; i++) if (c(i) != c(i-1)) different_cs++;
                //**/printf("estimatedAdderCost: %d\n", estimatedAdderCost(c));
                /*if (first_call || opt_convert_goal == opt_convert) {
  	            Int max_coeff = c(c.size-1);
  	            int max_log = 0; while ((max_coeff >>= 1) > 0) max_log++;
                    double add_sort_factor = (max_log > 20 ? 1.0/(max_log-20) : max_log <= 10 ? 15.0 : 22.0-max_log);
                    Formula result = buildConstraint(c, (int)(adder_cost * opt_sort_thres * add_sort_factor));
                    if (result == _undef_) {
                        result = convertToBdd(c, (int)(adder_cost * opt_bdd_thres));
                        if (result != _undef_) 
                            if (!first_call) opt_convert = ct_BDDs, ++bddOptEncodings; else ++bddEncodings;
                    } else if (!first_call) opt_convert = ct_Sorters, ++srtOptEncodings; else ++srtEncodings;
                    if (result == _undef_) linearAddition(c, converted_constrs), first_call ? ++addEncodings : ++addOptEncodings;
                    else converted_constrs.push(result);
                } else*/ {
                    Formula result = buildConstraint(c, (int)(adder_cost * opt_sort_thres)); 
                    if (result == _undef_) {
                        result = convertToBdd(c, (int)(adder_cost * opt_bdd_thres));
                        if (result != _undef_)
                            if (!first_call) opt_convert = ct_BDDs, ++bddOptEncodings; else ++bddEncodings; 
                    } else if (!first_call) opt_convert = ct_Sorters, ++srtOptEncodings; else ++srtEncodings;
                    if (result == _undef_) linearAddition(c, converted_constrs), first_call ? ++addEncodings : ++addOptEncodings;
                    else converted_constrs.push(result);
                }
            } else assert(false);
            constrs[i]->~Linear(); constrs[i] = NULL;
            if (!opt_shared_fmls && FEnv::nodes.size() >= 100000) { 
                clausify(sat_solver, converted_constrs); converted_constrs.clear(); 
            }
            opt_convert = saved_opt_convert;
        } catch (std::bad_alloc& ba) { // M. Piotrow 11.10.2017
	    FEnv::pop(); i-=1;
	    if (opt_convert == ct_Sorters)  { opt_convert = ct_BDDs; continue; }
	    if (opt_convert != ct_Adders)  { opt_convert = ct_Adders; continue; }
            else {
	        reportf("Out of memery in converting constraints: %s\n",ba.what());
	        exit(1); //throw(std::bad_alloc);
	    }
        }
        if (!okay()) return false;
    }
    constrs.clear();
    mem.clear();

    try { // M. Piotrow 15.06.2018
      clausify(sat_solver, converted_constrs);
    } catch (std::bad_alloc& ba) { // M. Piotrow 15.06.2018
      reportf("Out of memery in clausifying constraints: %s\n",ba.what());
      exit(1);
    }
    
    return okay();
}

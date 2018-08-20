/*-----------------------------------------------------------------------

  File  : cco_proofproc.c

  Author: Stephan Schulz

  Contents

  Functions realizing the proof procedure.

  Copyright 1998--2018 by the author.
  This code is released under the GNU General Public Licence and
  the GNU Lesser General Public License.
  See the file COPYING in the main E directory for details..
  Run "eprover -h" for contact information.

  Created: Mon Jun  8 11:47:44 MET DST 1998

  -----------------------------------------------------------------------*/

#include "cco_proofproc.h"



/*---------------------------------------------------------------------*/
/*                        Global Variables                             */
/*---------------------------------------------------------------------*/

PERF_CTR_DEFINE(ParamodTimer);
PERF_CTR_DEFINE(BWRWTimer);



/*---------------------------------------------------------------------*/
/*                      Forward Declarations                           */
/*---------------------------------------------------------------------*/

bool upperCase(char c);
bool integer(char c);
char* TSTPToString(Clause_p clause);
long compute_replacement(TB_p bank, OCB_p ocb, Clause_p clause,
              Clause_p parent_alias, ClauseSet_p
              with_set, ClauseSet_p store, VarBank_p
              freshvars, ParamodulationType pm_type, ProofState_p state);
char* CNFFreeVariables(char *input);
char* FOFFreeVariables(char *input);
char* FreeVariables(Clause_p clause);
void addFormulaToState(char* fname, ProofState_p state);
int count_characters(const char *str, char character);
char* Replacement(char *input);
char* NewVariables(char *inp);

/*---------------------------------------------------------------------*/
/*                         Internal Functions                          */
/*---------------------------------------------------------------------*/

/*-----------------------------------------------------------------------
//
// Function: document_processing()
//
//   Document processing of the new given clause (depending on the
//   output level).
//
// Global Variables: OutputLevel, GlobalOut (read only)
//
// Side Effects    : Output
//
/----------------------------------------------------------------------*/

void document_processing(Clause_p clause)
{
   if(OutputLevel)
   {
      if(OutputLevel == 1)
      {
         putc('\n', GlobalOut);
         putc('#', GlobalOut);
         ClausePrint(GlobalOut, clause, true);
         putc('\n', GlobalOut);
      }
      DocClauseQuoteDefault(6, clause, "new_given");
   }
}

/*-----------------------------------------------------------------------
//
// Function: check_ac_status()
//
//   Check if the AC theory has been extended by the currently
//   processed clause, and act accordingly.
//
// Global Variables: -
//
// Side Effects    : Changes AC status in signature
//
/----------------------------------------------------------------------*/

static void check_ac_status(ProofState_p state, ProofControl_p
                            control, Clause_p clause)
{
   if(control->heuristic_parms.ac_handling!=NoACHandling)
   {
      bool res;
      res = ClauseScanAC(state->signature, clause);
      if(res && !control->ac_handling_active)
      {
         control->ac_handling_active = true;
         if(OutputLevel)
         {
            SigPrintACStatus(GlobalOut, state->signature);
            fprintf(GlobalOut, "# AC handling enabled dynamically\n");
         }
      }
   }
}


/*-----------------------------------------------------------------------
//
// Function: remove_subsumed()
//
//   Remove all clauses subsumed by subsumer from set, kill their
//   children. Return number of removed clauses.
//
// Global Variables: -
//
// Side Effects    : Changes set, memory operations.
//
/----------------------------------------------------------------------*/

static long remove_subsumed(GlobalIndices_p indices,
                            FVPackedClause_p subsumer,
                            ClauseSet_p set,
                            ClauseSet_p archive)
{
   Clause_p handle;
   long     res;
   PStack_p stack = PStackAlloc();

   res = ClauseSetFindFVSubsumedClauses(set, subsumer, stack);

   while(!PStackEmpty(stack))
   {
      handle = PStackPopP(stack);
      // printf("# XXX Removing (remove_subumed()) %p from %p = %p\n", handle, set, handle->set);
      if(ClauseQueryProp(handle, CPWatchOnly))
      {
         DocClauseQuote(GlobalOut, OutputLevel, 6, handle,
                        "extract_wl_subsumed", subsumer->clause);

      }
      else
      {
         DocClauseQuote(GlobalOut, OutputLevel, 6, handle,
                        "subsumed", subsumer->clause);
      }
      ClauseKillChildren(handle);
      GlobalIndicesDeleteClause(indices, handle);
      ClauseSetExtractEntry(handle);
      ClauseSetInsert(archive, handle);
   }
   PStackFree(stack);
   return res;
}


/*-----------------------------------------------------------------------
//
// Function: eliminate_backward_rewritten_clauses()
//
//   Remove all processed clauses rewritable with clause and put
//   them into state->tmp_store.
//
// Global Variables: -
//
// Side Effects    : Changes clause sets
//
/----------------------------------------------------------------------*/

static bool
eliminate_backward_rewritten_clauses(ProofState_p
                                     state, ProofControl_p control,
                                     Clause_p clause, SysDate *date)
{
   long old_lit_count = state->tmp_store->literals,
      old_clause_count= state->tmp_store->members;
   bool min_rw = false;

   PERF_CTR_ENTRY(BWRWTimer);
   if(ClauseIsDemodulator(clause))
   {
      SysDateInc(date);
      if(state->gindices.bw_rw_index)
      {
         min_rw = RemoveRewritableClausesIndexed(control->ocb,
                                                 state->tmp_store,
                                                 state->archive,
                                                 clause, *date, &(state->gindices));

      }
      else
      {
         min_rw = RemoveRewritableClauses(control->ocb,
                                          state->processed_pos_rules,
                                          state->tmp_store,
                                          state->archive,
                                          clause, *date, &(state->gindices))
            ||min_rw;
         min_rw = RemoveRewritableClauses(control->ocb,
                                          state->processed_pos_eqns,
                                          state->tmp_store,
                                          state->archive,
                                          clause, *date, &(state->gindices))
            ||min_rw;
         min_rw = RemoveRewritableClauses(control->ocb,
                                          state->processed_neg_units,
                                          state->tmp_store,
                                          state->archive,
                                          clause, *date, &(state->gindices))
            ||min_rw;
         min_rw = RemoveRewritableClauses(control->ocb,
                                          state->processed_non_units,
                                          state->tmp_store,
                                          state->archive,
                                          clause, *date, &(state->gindices))
            ||min_rw;
      }
      state->backward_rewritten_lit_count+=
         (state->tmp_store->literals-old_lit_count);
      state->backward_rewritten_count+=
         (state->tmp_store->members-old_clause_count);

      if(control->heuristic_parms.detsort_bw_rw)
      {
         ClauseSetSort(state->tmp_store, ClauseCmpByStructWeight);
      }
   }
   PERF_CTR_EXIT(BWRWTimer);
   /*printf("# Removed %ld clauses\n",
     (state->tmp_store->members-old_clause_count)); */
   return min_rw;
}


/*-----------------------------------------------------------------------
//
// Function: eliminate_backward_subsumed_clauses()
//
//   Eliminate subsumed processed clauses, return number of clauses
//   deleted.
//
// Global Variables: -
//
// Side Effects    : Changes clause sets.
//
/----------------------------------------------------------------------*/

static long eliminate_backward_subsumed_clauses(ProofState_p state,
                                                FVPackedClause_p pclause)
{
   long res = 0;

   if(ClauseLiteralNumber(pclause->clause) == 1)
   {
      if(pclause->clause->pos_lit_no)
      {
         /* A unit rewrite rule that is a variant of an old rule is
            already subsumed by the older one.
            A unit rewrite rule can never subsume an unorientable
            equation (else it would be unorientable itself). */
         if(!ClauseIsRWRule(pclause->clause))
         {
            res += remove_subsumed(&(state->gindices), pclause,
                                   state->processed_pos_rules,
                                   state->archive);
            res += remove_subsumed(&(state->gindices), pclause,
                                   state->processed_pos_eqns,
                                   state->archive);
         }
         res += remove_subsumed(&(state->gindices), pclause,
                                state->processed_non_units,
                                state->archive);
      }
      else
      {
         res += remove_subsumed(&(state->gindices), pclause,
                                state->processed_neg_units,
                                state->archive);
         res += remove_subsumed(&(state->gindices), pclause,
                                state->processed_non_units,
                                state->archive);
      }
   }
   else
   {
      res += remove_subsumed(&(state->gindices), pclause,
                             state->processed_non_units,
                             state->archive);
   }
   state->backward_subsumed_count+=res;
   return res;
}



/*-----------------------------------------------------------------------
//
// Function: eliminate_unit_simplified_clauses()
//
//   Perform unit-back-simplification on the proof state.
//
// Global Variables: -
//
// Side Effects    : Potentially changes and moves clauses.
//
/----------------------------------------------------------------------*/

static void eliminate_unit_simplified_clauses(ProofState_p state,
                                              Clause_p clause)
{
   if(ClauseIsRWRule(clause)||!ClauseIsUnit(clause))
   {
      return;
   }
   ClauseSetUnitSimplify(state->processed_non_units, clause,
                         state->tmp_store,
                         state->archive,
                         &(state->gindices));
   if(ClauseIsPositive(clause))
   {
      ClauseSetUnitSimplify(state->processed_neg_units, clause,
                            state->tmp_store,
                            state->archive,
                            &(state->gindices));
   }
   else
   {
      ClauseSetUnitSimplify(state->processed_pos_rules, clause,
                            state->tmp_store,
                            state->archive,
                            &(state->gindices));
      ClauseSetUnitSimplify(state->processed_pos_eqns, clause,
                            state->tmp_store,
                            state->archive,
                            &(state->gindices));
   }
}

/*-----------------------------------------------------------------------
//
// Function: eliminate_context_sr_clauses()
//
//   If required by control, remove all
//   backward-contextual-simplify-reflectable clauses.
//
// Global Variables: -
//
// Side Effects    : Moves clauses from state->processed_non_units
//                   to state->tmp_store
//
/----------------------------------------------------------------------*/

static long eliminate_context_sr_clauses(ProofState_p state,
                                         ProofControl_p control,
                                         Clause_p clause)
{
   if(!control->heuristic_parms.backward_context_sr)
   {
      return 0;
   }
   return RemoveContextualSRClauses(state->processed_non_units,
                                    state->tmp_store,
                                    state->archive,
                                    clause,
                                    &(state->gindices));
}

/*-----------------------------------------------------------------------
//
// Function: check_watchlist()
//
//   Check if a clause subsumes one or more watchlist clauses, if yes,
//   set appropriate property in clause and remove subsumed clauses.
//
// Global Variables: -
//
// Side Effects    : As decribed.
//
/----------------------------------------------------------------------*/

void check_watchlist(GlobalIndices_p indices, ClauseSet_p watchlist,
                     Clause_p clause, ClauseSet_p archive,
                     bool static_watchlist)
{
   FVPackedClause_p pclause = FVIndexPackClause(clause, watchlist->fvindex);
   long removed;

   // printf("# check_watchlist(%p)...\n", indices);
   ClauseSubsumeOrderSortLits(clause);
   // assert(ClauseIsSubsumeOrdered(clause));

   clause->weight = ClauseStandardWeight(clause);

   if(static_watchlist)
   {
      Clause_p subsumed;

      subsumed = ClauseSetFindFirstSubsumedClause(watchlist, clause);
      if(subsumed)
      {
         ClauseSetProp(clause, CPSubsumesWatch);
      }
   }
   else
   {
      if((removed = remove_subsumed(indices, pclause, watchlist, archive)))
      {
         ClauseSetProp(clause, CPSubsumesWatch);
         if(OutputLevel == 1)
         {
            fprintf(GlobalOut,"# Watchlist reduced by %ld clause%s\n",
                    removed,removed==1?"":"s");
         }
         // ClausePrint(GlobalOut, clause, true); printf("\n");
         DocClauseQuote(GlobalOut, OutputLevel, 6, clause,
                        "extract_subsumed_watched", NULL);   }
   }
   FVUnpackClause(pclause);
   // printf("# ...check_watchlist()\n");
}


/*-----------------------------------------------------------------------
//
// Function: simplify_watchlist()
//
//   Simplify all clauses in state->watchlist with processed positive
//   units from state. Assumes that all those clauses are in normal
//   form with respect to all clauses but clause!
//
// Global Variables: -
//
// Side Effects    : Changes watchlist, introduces new rewrite links
//                   into the term bank!
//
/----------------------------------------------------------------------*/

void simplify_watchlist(ProofState_p state, ProofControl_p control,
                        Clause_p clause)
{
   ClauseSet_p tmp_set;
   Clause_p handle;
   long     removed_lits;

   if(!ClauseIsDemodulator(clause))
   {
      return;
   }
   // printf("# simplify_watchlist()...\n");
   tmp_set = ClauseSetAlloc();

   if(state->wlindices.bw_rw_index)
   {
      // printf("# Simpclause: "); ClausePrint(stdout, clause, true); printf("\n");
      RemoveRewritableClausesIndexed(control->ocb,
                                     tmp_set, state->archive,
                                     clause, clause->date,
                                     &(state->wlindices));
      // printf("# Simpclause done\n");
   }
   else
   {
      RemoveRewritableClauses(control->ocb, state->watchlist,
                              tmp_set, state->archive,
                              clause, clause->date,
                              &(state->wlindices));
   }
   while((handle = ClauseSetExtractFirst(tmp_set)))
   {
      // printf("# WL simplify: "); ClausePrint(stdout, handle, true);
      // printf("\n");
      ClauseComputeLINormalform(control->ocb,
                                state->terms,
                                handle,
                                state->demods,
                                control->heuristic_parms.forward_demod,
                                control->heuristic_parms.prefer_general);
      removed_lits = ClauseRemoveSuperfluousLiterals(handle);
      if(removed_lits)
      {
         DocClauseModificationDefault(handle, inf_minimize, NULL);
      }
      if(control->ac_handling_active)
      {
         ClauseRemoveACResolved(handle);
      }
      handle->weight = ClauseStandardWeight(handle);
      ClauseMarkMaximalTerms(control->ocb, handle);
      ClauseSetIndexedInsertClause(state->watchlist, handle);
      // printf("# WL Inserting: "); ClausePrint(stdout, handle, true); printf("\n");
      GlobalIndicesInsertClause(&(state->wlindices), handle);
   }
   ClauseSetFree(tmp_set);
   // printf("# ...simplify_watchlist()\n");
}

/*  Returns four new TPTP variables not occuring in the given list of variables
 * 
 *  John Hester
 * 
 * 
 * 
*/

char *NewVariables(char *inp)
{
	char *newvars = calloc(100, sizeof(char));
	char c = inp[0];
	char zero = '0';
	char max = zero;  //we will use a number identifier larger than any other to ensure that the variable is fresh
	int size = strlen(inp);
	for (int i = 0; i<size;i++)
	{
		if ((inp[i] >= zero &&) (integer(inp[i]) == true))
		{
			max = inp[i];
		}
	}
	return newvars;
}


/*  Replace substring with another...
 * 
 * 
 * 
 * 
 * 
*/

char *replace_str(char *str, char *orig, char *rep)
{
  static char buffer[4096];
  char *p;

  if(!(p = strstr(str, orig)))  // Is 'orig' even in 'str'?
    return str;

  strncpy(buffer, str, p-str); // Copy characters from 'str' start to 'orig' st$
  buffer[p-str] = '\0';

  sprintf(buffer+(p-str), "%s%s", rep, p+strlen(orig));

  return buffer;
}

/* Counts the characters in a string...
 * 
*/

int count_characters(const char *str, char character)
{
    const char *p = str;
    int count = 0;

    do {
        if (*p == character)
            count++;
    } while (*(p++));

    return count;
}

/* John Hester
 * 
 * Check if character is uppercase...
 * 
 * 
*/

bool upperCase(char c)
{
	if (c >= 'A' && c <= 'Z') {
		return true;
	}
	return false;
}

/* John Hester
 * 
 * Check if character is integer...
 * 
 * 
*/

bool integer(char c)
{
	if (c >= '0' && c <= '9') {
		return true;
	}
	return false;
}

/*  Actually creates the parameter-free replacement instance
 *  First checks if the input has exactly two free variables, then builds it
 *  Returns null if the inference is not possible, else returns the REPL0 string
 * 
 *  John Hester
 * 
*/

char* Replacement(char *input) {
	char *variables = CNFFreeVariables(input);
	
	int count = count_characters(variables, ',');
	if (count != 1) 
	{
		return NULL;
	}
	else 
	{
		printf("\nTwo free variables!\n");
		char var1[3] = {variables[0],variables[1]};
		char var2[3] = {variables[3],variables[4]};
		printf("var1: %s\n",var1);
		printf("var2: %s\n",var2);
		return input;
	}
}


/*  Returns a comma separated string of the FREE variables of a FOF expression
 *  Only call if it's FOF!!!
 * 
 * 
 * 
 * 
*/

char* FOFFreeVariables(char *input)
{
	char *output = malloc(sizeof(char) * 1);
	int capacity = 1;
	int length_of_formula = 0;
	int size = strlen(input);
	bool reading = false;
	
	//printf("\nsize: %d\n", size);
	if (input[0]=='f') 
	{
		printf("\nfof\n");
		printf("\n%s\n",input);
		for (int i = 3;i<size;i++) {
			if (input[i] == '.' || input[i] == ']') break;
			if (input[i] == '!') reading = true;
			if (upperCase(input[i]) == true && reading == true) 
			{
				printf("\n capital: %c\n",input[i]);
				if (capacity == length_of_formula)
					{
						capacity = 2 * length_of_formula;
						output = realloc(output, capacity * sizeof(char));
					}
				output[length_of_formula] = input[i];
				length_of_formula += 1;
				// Now we need to check for trailing integer variable identifiers and add them to output
				for (int d = i+1;d<size; d++) 
				{
					if (integer(input[d]) == true) 
					{
						if (capacity == length_of_formula)
						{
							capacity = 2 * length_of_formula;
							output = realloc(output, capacity * sizeof(char));
						}
						output[length_of_formula] = input[d];
						length_of_formula += 1;
						printf("added integer variable ident\n");
					}
					else 
					{
						if (capacity == length_of_formula)
						{
							capacity = 2 * length_of_formula;
							output = realloc(output, capacity * sizeof(char));
						}
						output[length_of_formula] = ',';
						length_of_formula += 1;
						break;						
					}
					
				}
				
			}
			// At this point we should have found all of the variables with identifiers and put it all in to a comma separated list
			// Null terminate it
			if (capacity == length_of_formula)
			{
				capacity = length_of_formula+1;
				output = realloc(output, capacity * sizeof(char));
			}
			output[length_of_formula] = 0;
		}
	}
	else
	{
		printf("\nCalling FOF free variables on something that is not a fof...\n");
	}
	
	printf("\n size of clause: %d\n",size);
	printf("This is the string of variables found: %s\n",output);
	
	////////////////////////////////////////////////
	// This deletes all duplicates... Risk of overflow if the collection of variables ends up being of size more than 100
	char *final = malloc(sizeof(char) * 100);
	
	const char s[2] = ",";
	char *token;
    token = strtok(output, s);
    while( token != NULL ) 
    {
		if (strstr(final,token) == 0)
		{
			char *p = ",";
			strcat(final,token);
			strcat(final,p);
		}
		//printf( " %s\n", token );
		token = strtok(NULL, s);
	}
	//  null terminate the string to be safe
	int length = strlen(final);
	//printf("%c\n",final[length-1]);
	final[length-1]=0;
	
	//printf("This is the string of variables found, with duplicates removed: %s\n",final);
	
	free(output);
	return final;
}

/*  Returns a comma separated string of the FREE variables of a CNF expression
 *  Only call if it's CNF!!!
 * 
 *  John Hester
 * 
 * 
*/

char* CNFFreeVariables(char *input)
{
	char *output = malloc(sizeof(char) * 1);
	int capacity = 1;
	int length_of_formula = 0;
	int size = strlen(input);
	
	//printf("\nsize: %d\n", size);
	if (input[0]=='c') 
	{
		printf("\ncnf\n");
		printf("\n%s\n",input);
		for (int i = 3;i<size;i++) {
			if (input[i] == '.') break;
			else if (upperCase(input[i]) == true) 
			{
				printf("\n capital: %c\n",input[i]);
				if (capacity == length_of_formula)
					{
						capacity = 2 * length_of_formula;
						output = realloc(output, capacity * sizeof(char));
					}
				output[length_of_formula] = input[i];
				length_of_formula += 1;
				// Now we need to check for trailing integer variable identifiers and add them to output
				for (int d = i+1;d<size; d++) 
				{
					if (integer(input[d]) == true) 
					{
						if (capacity == length_of_formula)
						{
							capacity = 2 * length_of_formula;
							output = realloc(output, capacity * sizeof(char));
						}
						output[length_of_formula] = input[d];
						length_of_formula += 1;
						printf("added integer variable ident\n");
					}
					else 
					{
						if (capacity == length_of_formula)
						{
							capacity = 2 * length_of_formula;
							output = realloc(output, capacity * sizeof(char));
						}
						output[length_of_formula] = ',';
						length_of_formula += 1;
						break;						
					}
					
				}
				
			}
			// At this point we should have found all of the variables with identifiers and put it all in to a comma separated list
			// Null terminate it
			if (capacity == length_of_formula)
			{
				capacity = length_of_formula+1;
				output = realloc(output, capacity * sizeof(char));
			}
			output[length_of_formula] = 0;
		}
	}
	else
	{
		printf("\nCalling CNF free variables on something that is not a cnf...\n");
	}
	
	//printf("\n size of clause: %d\n",size);
	printf("Has this string of variables discovered hieroglyphics?: %s\n",output);
	
	////////////////////////////////////////////////
	// This deletes all duplicates... Risk of overflow if the collection of variables ends up being of size more than 100
	char *final = calloc(100, sizeof(char));
	
	const char s[2] = ",";
	char *token;
    token = strtok(output, s);
    printf("token: %s\n", token);

	while( token != NULL ) 
	{
		if (strstr(final,token) == 0)
		{
			char *p = ",";
			strcat(final,token);
			strcat(final,p);
		}
		//printf( " %s\n", token );
		token = strtok(NULL, s);
	}
	//  null terminate the string to be safe
	int length = strlen(final);
	//printf("%c\n",final[length-1]);
	printf("Final: %s\n",final);
	
	final[length-1]=0;
	printf("Final2: %s\n",final);
	//printf("This is the string of variables found, with duplicates removed: %s\n",final);
	
	free(output);
	return final;
}

/* John Hester
 * 
 *  Takes an input formula in TPTP fromat and returns the symbols for "free" variables
 *  Here "free" for fof means universally quantified over with scope the entire formula
 *  For cnf free means not quantified over...
 * 
 *  The return type is a comma separated string
 *
*/

char* FreeVariables(Clause_p clause)
{
	
	char *input = TSTPToString(clause);
	char *output;
	
	//printf("\nsize: %d\n", size);
	if (input[0]=='c') 
	{
		output = CNFFreeVariables(input);
	}
	else if (input[0]=='f')
	{
		output = FOFFreeVariables(input);
	}
	else
	{
		printf("\nError in variables!!\n");
	}
	
	
	printf("This is the string of variables found, with duplicates removed: %s\n",output);
	
	return output;
}

/* John Hester
 * 
 * Prints clause to a file in TPTP format, reads it as a string, returns it for manipulation
 * 
 * 
 * 
 * 
 *
*/

char* TSTPToString(Clause_p clause) 
{
	//printf("computing all comprehensions call test\n");
	FILE *fp = fopen("currentformula.txt", "wb+");
	ClauseTSTPPrint(fp,clause,1,1);  //This is necessary because it is where we read the current clause from!
	fprintf(fp,"\n");
	
	rewind(fp);
	
	char *formula = malloc(sizeof(char) * 1);
	int capacity = 1;
	int length_of_formula = 0;
	int terminate_string_with_null = 0;
	
	/////////////////////////  Read the current formula that has been printed to fp

	do
	{
		if (capacity == length_of_formula)
		{
			capacity = 2 * length_of_formula;
			formula = realloc(formula, capacity * sizeof(char));
		}
		if (terminate_string_with_null == 1)
		{
			formula[length_of_formula] = 0;
			//printf("\nRead end of input formula\n");
			break;
		}
		int ch = fgetc(fp);
		if (ch == -1) 
		{
			if (formula[length_of_formula-1] != '.')
			{
				//printf("\nRead end of file without a preceding period!!!!\n");
			}
		}
		char casted = (char) ch;
		formula[length_of_formula] = casted;
		if (casted == '.')
		{
			terminate_string_with_null = 1;
			//printf("\nRead terminating period\n");
		}
		length_of_formula += 1;
	}
	while(1);
	fclose(fp);
	
	//printf("Length of formula: %d\n",length_of_formula);
	
	//printf("\nThis is the formula that has been read after printing: %s\n", formula);
	return formula;
}

/*  John Hester
 * 
 *  Reads target file determined by fname and adds the TPTP format clauses/formulas in them to the axioms of state
 *  Does NOT add to state->tmp_store, which would be ideal...
 * 
 *
 * 
*/

void addFormulaToState(char* fname, ProofState_p state) 
{

	Scanner_p in;
	StrTree_p skip_includes = NULL;
	in = CreateScanner(StreamTypeFile,fname,true,NULL);
	IOFormat format = TSTPFormat;
	ScannerSetFormat(in, format);
	//printf("\nHow many axioms are in state? %ld\n",state->axioms->members);
    FormulaAndClauseSetParse(in,
                               state->f_axioms,
                               state->watchlist,
                               state->terms,
                               NULL,
                               &skip_includes);
    CheckInpTok(in, NoToken);
    DestroyScanner(in);
	//  Lets check if the dumpster is on fire
	
	//FILE *fd = fopen("sanitycheck.txt","ab+");
	
	long cnf_size;
    cnf_size = FormulaSetCNF(state->f_axioms,
                               state->f_ax_archive,
                               state->axioms,
                               state->terms,
                               state->freshvars,
                               state->gc_terms);
    //ClauseSetTSTPPrint(fd,state->axioms,true);
}


/*	Compute ALL ZFC comprehension instances with the given clause
 *  John Hester
 * 
 * 	Parts Taken from ClauseTSTPPrint in order to get string
 * 
 *  WFormulaTPTPParse provides input in to WFormula_p format
 *  WFormClauseToClause provides transformation from WFormula_p to Clause_p format
 *  Both in ccl_formula_wrapper.c
 * look at TBPrintTermFull, TBPrintTerm
 * 
 *  parent alias seems to be same as clause but with different name for variables
*/

long compute_replacement(TB_p bank, OCB_p ocb, Clause_p clause,
              Clause_p parent_alias, ClauseSet_p
              with_set, ClauseSet_p store, VarBank_p
              freshvars, ParamodulationType pm_type, ProofState_p state) 
{
	
	char *formula = TSTPToString(clause);
	char *input_axiom;
	input_axiom = replace_str(formula,"plain","axiom");
	
	char *test = FreeVariables(clause);
	
	//printf(input_axiom);
	
	free(formula);
	free(test);
	
	char *fname = "processedclauses.txt";
	FILE *fc = fopen(fname, "ab+");
	fprintf(fc,"%s\n",input_axiom);
	fclose(fc);
	
	addFormulaToState(fname,state);
	
	Replacement(input_axiom);
	//free(input_axiom);
	
	//printf("\nHow many axioms are now in state? %ld\n",state->axioms->members);
	
	remove("processedclauses.txt");
	remove("currentformula.txt");
	//printf("\nleaving method\n");
	return 1;
	
}

/*-----------------------------------------------------------------------
//
// Function: generate_new_clauses()
//
//   Apply the generating inferences to the proof state, putting new
//   clauses into state->tmp_store.
//
// Global Variables: -
//
// Side Effects    : Changes proof state as described.
//
/----------------------------------------------------------------------*/

static void generate_new_clauses(ProofState_p state, ProofControl_p
                                 control, Clause_p clause, Clause_p tmp_copy)
{
 /////////////////////////////////////////////////////////////////////////
	int number_of_terms = TBTermNodes(state->terms);
	//printf("number of term nodes?: %d\n", number_of_terms);
	
	state->paramod_count += compute_replacement(state->terms, control->ocb,
                                    tmp_copy, clause,
                                    state->processed_pos_rules,
                                    state->tmp_store, state->freshvars,
                                    control->heuristic_parms.pm_type,
                                    state);
    
///////////////////////////////////////////////////////////////////////
   if(control->heuristic_parms.enable_eq_factoring)
   {
      state->factor_count+=
         ComputeAllEqualityFactors(state->terms, control->ocb, clause,
                                   state->tmp_store, state->freshvars);
   }
   state->resolv_count+=
      ComputeAllEqnResolvents(state->terms, clause, state->tmp_store,
                              state->freshvars);

   if(control->heuristic_parms.enable_neg_unit_paramod
      ||!ClauseIsUnit(clause)
      ||!ClauseIsNegative(clause))
   { /* Sometime we want to disable paramodulation for negative units */
      PERF_CTR_ENTRY(ParamodTimer);
      if(state->gindices.pm_into_index)
      {
         state->paramod_count+=
            ComputeAllParamodulantsIndexed(state->terms,
                                           control->ocb,
                                           state->freshvars,
                                           tmp_copy,
                                           clause,
                                           state->gindices.pm_into_index,
                                           state->gindices.pm_negp_index,
                                           state->gindices.pm_from_index,
                                           state->tmp_store,
                                           control->heuristic_parms.pm_type);
      }
      else
      {
         state->paramod_count+=
            ComputeAllParamodulants(state->terms, control->ocb,
                                    tmp_copy, clause,
                                    state->processed_pos_rules,
                                    state->tmp_store, state->freshvars,
                                    control->heuristic_parms.pm_type);
                                    
         state->paramod_count+=
            ComputeAllParamodulants(state->terms, control->ocb,
                                    tmp_copy, clause,
                                    state->processed_pos_eqns,
                                    state->tmp_store, state->freshvars,
                                    control->heuristic_parms.pm_type);
                                    

         if(control->heuristic_parms.enable_neg_unit_paramod && !ClauseIsNegative(clause))
         { /* We never need to try to overlap purely negative clauses! */
            state->paramod_count+=
               ComputeAllParamodulants(state->terms, control->ocb,
                                       tmp_copy, clause,
                                       state->processed_neg_units,
                                       state->tmp_store, state->freshvars,
                                       control->heuristic_parms.pm_type);
         }
         state->paramod_count+=
            ComputeAllParamodulants(state->terms, control->ocb,
                                    tmp_copy, clause,
                                    state->processed_non_units,
                                    state->tmp_store, state->freshvars,
                                    control->heuristic_parms.pm_type);
      }
      PERF_CTR_EXIT(ParamodTimer);
   }
}


/*-----------------------------------------------------------------------
//
// Function: eval_clause_set()
//
//   Add evaluations to all clauses in state->eval_set. Factored out
//   so that batch-processing with e.g. neural networks can be easily
//   integrated.
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/

void eval_clause_set(ProofState_p state, ProofControl_p control)
{
   Clause_p handle;
   assert(state);
   assert(control);

   for(handle = state->eval_store->anchor->succ;
       handle != state->eval_store->anchor;
       handle = handle->succ)
   {
      HCBClauseEvaluate(control->hcb, handle);
   }
}


/*-----------------------------------------------------------------------
//
// Function: insert_new_clauses()
//
//   Rewrite clauses in state->tmp_store, remove superfluous literals,
//   insert them into state->unprocessed. If an empty clause is
//   detected, return it, otherwise return NULL.
//
// Global Variables: -
//
// Side Effects    : As described.
//
/----------------------------------------------------------------------*/

static Clause_p insert_new_clauses(ProofState_p state, ProofControl_p control)
{
   Clause_p handle;
   long     clause_count;

   state->generated_count+=state->tmp_store->members;
   state->generated_lit_count+=state->tmp_store->literals;
   while((handle = ClauseSetExtractFirst(state->tmp_store)))
   {
      /* printf("Inserting: ");
         ClausePrint(stdout, handle, true);
         printf("\n"); */
      if(ClauseQueryProp(handle,CPIsIRVictim))
      {
         assert(ClauseQueryProp(handle, CPLimitedRW));
         ForwardModifyClause(state, control, handle,
                             control->heuristic_parms.forward_context_sr_aggressive||
                             (control->heuristic_parms.backward_context_sr&&
                              ClauseQueryProp(handle,CPIsProcessed)),
                             control->heuristic_parms.condensing_aggressive,
                             FullRewrite);
         ClauseDelProp(handle,CPIsIRVictim);
      }
      ForwardModifyClause(state, control, handle,
                          control->heuristic_parms.forward_context_sr_aggressive||
                          (control->heuristic_parms.backward_context_sr&&
                           ClauseQueryProp(handle,CPIsProcessed)),
                          control->heuristic_parms.condensing_aggressive,
                          control->heuristic_parms.forward_demod);


      if(ClauseIsTrivial(handle))
      {
         assert(!handle->children);
         ClauseDetachParents(handle);
         ClauseFree(handle);
         continue;
      }
      if(state->watchlist)
      {
         check_watchlist(&(state->wlindices), state->watchlist,
                         handle, state->archive,
                         control->heuristic_parms.watchlist_is_static);
      }
      if(ClauseIsEmpty(handle))
      {
         return handle;
      }
      if(control->heuristic_parms.er_aggressive &&
         control->heuristic_parms.er_varlit_destructive &&
         (clause_count =
          ClauseERNormalizeVar(state->terms,
                               handle,
                               state->tmp_store,
                               state->freshvars,
                               control->heuristic_parms.er_strong_destructive)))
      {
         state->other_redundant_count += clause_count;
         state->resolv_count += clause_count;
         state->generated_count += clause_count;
         continue;
      }
      if(control->heuristic_parms.split_aggressive &&
         (clause_count = ControlledClauseSplit(state->definition_store,
                                               handle,
                                               state->tmp_store,
                                               control->heuristic_parms.split_clauses,
                                               control->heuristic_parms.split_method,
                                               control->heuristic_parms.split_fresh_defs)))
      {
         state->generated_count += clause_count;
         continue;
      }
      state->non_trivial_generated_count++;
      ClauseDelProp(handle, CPIsOriented);
      if(!control->heuristic_parms.select_on_proc_only)
      {
         DoLiteralSelection(control, handle);
      }
      else
      {
         EqnListDelProp(handle->literals, EPIsSelected);
      }
      handle->create_date = state->proc_non_trivial_count;
      if(ProofObjectRecordsGCSelection)
      {
         ClausePushDerivation(handle, DCCnfEvalGC, NULL, NULL);
      }
//      HCBClauseEvaluate(control->hcb, handle);

      ClauseSetInsert(state->eval_store, handle);
   }
   eval_clause_set(state, control);

   while((handle = ClauseSetExtractFirst(state->eval_store)))
   {
      ClauseDelProp(handle, CPIsOriented);
      DocClauseQuoteDefault(6, handle, "eval");

      ClauseSetInsert(state->unprocessed, handle);
   }
   return NULL;
}


/*-----------------------------------------------------------------------
//
// Function: replacing_inferences()
//
//   Perform the inferences that replace a clause by another:
//   Destructive equality-resolution and/or splitting.
//
//   Returns NULL if clause was replaced, the empty clause if this
//   produced an empty clause, and the original clause otherwise
//
// Global Variables: -
//
// Side Effects    : May insert new clauses into state. May destroy
//                   pclause (in which case it gets rid of the container)
//
/----------------------------------------------------------------------*/

Clause_p replacing_inferences(ProofState_p state, ProofControl_p
                              control, FVPackedClause_p pclause)
{
   long     clause_count;
   Clause_p res = pclause->clause;

   if(control->heuristic_parms.er_varlit_destructive &&
      (clause_count =
       ClauseERNormalizeVar(state->terms,
                            pclause->clause,
                            state->tmp_store,
                            state->freshvars,
                            control->heuristic_parms.er_strong_destructive)))
   {
      state->other_redundant_count += clause_count;
      state->resolv_count += clause_count;
      pclause->clause = NULL;
   }
   else if(ControlledClauseSplit(state->definition_store,
                                 pclause->clause, state->tmp_store,
                                 control->heuristic_parms.split_clauses,
                                 control->heuristic_parms.split_method,
                                 control->heuristic_parms.split_fresh_defs))
   {
      pclause->clause = NULL;
   }

   if(!pclause->clause)
   {  /* ...then it has been destroyed by one of the above methods,
       * which may have put some clauses into tmp_store. */
      FVUnpackClause(pclause);

      res = insert_new_clauses(state, control);
   }
   return res;
}


/*-----------------------------------------------------------------------
//
// Function: cleanup_unprocessed_clauses()
//
//   Perform maintenenance operations on state->unprocessed, depending
//   on paramters in control:
//   - Remove copies
//   - Simplify all unprocessed clauses
//   - Reweigh all unprocessed clauses
//   - Delete "bad" clauses to avoid running out of memories.
//   Simplification can find the empty clause, which is then
//   returned.
//
// Global Variables: -
//
// Side Effects    : As described above.
//
/----------------------------------------------------------------------*/

static Clause_p cleanup_unprocessed_clauses(ProofState_p state,
                                            ProofControl_p control)
{
   long long current_storage    = ProofStateStorage(state);
   long long filter_base        = current_storage;
   long long filter_copies_base = current_storage;
   long long reweight_base      = state->unprocessed->members;
   long tmp;
   Clause_p unsatisfiable = NULL;

   filter_copies_base = MIN(filter_copies_base,current_storage);
   if((current_storage - filter_copies_base) >
      control->heuristic_parms.filter_copies_limit)
   {
      tmp = ClauseSetDeleteCopies(state->unprocessed);
      if(OutputLevel)
      {
         fprintf(GlobalOut,
                 "# Deleted %ld clause copies (remaining: %ld)\n",
                 tmp, state->unprocessed->members);
      }
      state->other_redundant_count += tmp;
      current_storage  = ProofStateStorage(state);
      filter_copies_base = current_storage;
   }
   filter_base = MIN(filter_base,current_storage);
   if((current_storage - filter_base) > control->heuristic_parms.filter_limit)
   {
      tmp = state->unprocessed->members;
      unsatisfiable =
         ForwardContractSet(state, control,
                            state->unprocessed, false, FullRewrite,
                            &(state->other_redundant_count), true);
      if(unsatisfiable)
      {
         PStackPushP(state->extract_roots, unsatisfiable);
      }
      if(OutputLevel)
      {
         fprintf(GlobalOut,
                 "# Special forward-contraction deletes %ld clauses"
                 "(remaining: %ld) \n",
                 tmp - state->unprocessed->members,
                 state->unprocessed->members);
      }
      current_storage  = ProofStateStorage(state);
      filter_base = current_storage;
      if(unsatisfiable)
      {
         return unsatisfiable;
      }
   }
   reweight_base = MIN(state->unprocessed->members, reweight_base);
   if((state->unprocessed->members - reweight_base)
      > control->heuristic_parms.reweight_limit)
   {
      OUTPRINT(1, "# Reweighting unprocessed clauses...\n");
      ClauseSetReweight(control->hcb,  state->unprocessed);
      reweight_base = state->unprocessed->members;
   }
   tmp = LONG_MAX;

   if(current_storage > control->heuristic_parms.delete_bad_limit)
   {
      tmp = HCBClauseSetDeleteBadClauses(control->hcb,
                                         state->unprocessed,
                                         state->unprocessed->members/2);
      state->non_redundant_deleted += tmp;
      if(OutputLevel)
      {
         fprintf(GlobalOut,
                 "# Deleted %ld bad clauses (prover may be"
                 " incomplete now)\n", tmp);
      }
      state->state_is_complete = false;
//       ProofStateGCMarkTerms(state);
//       ProofStateGCSweepTerms(state);
      GCCollect(state->terms->gc);
      current_storage = ProofStateStorage(state);
      filter_base = MIN(filter_base, current_storage);
      filter_copies_base = MIN(filter_copies_base, current_storage);
   }
   return unsatisfiable;
}

/*-----------------------------------------------------------------------
//
// Function: SATCheck()
//
//   Create ground (or pseudo-ground) instances of the clause set,
//   hand them to a SAT solver, and check then for unsatisfiability.
//
// Global Variables:
//
// Side Effects    :
//
/----------------------------------------------------------------------*/



Clause_p SATCheck(ProofState_p state, ProofControl_p control)
{
   Clause_p     empty = NULL;
   ProverResult res;

   if(control->heuristic_parms.sat_check_normalize)
   {
      //printf("# Cardinality of unprocessed: %ld\n",
      //        ClauseSetCardinality(state->unprocessed));
      empty = ForwardContractSetReweight(state, control, state->unprocessed,
                                       false, 2,
                                       &(state->proc_trivial_count));
      // printf("# ForwardContraction done\n");

   }
   if(!empty)
   {
      SatClauseSet_p set = SatClauseSetAlloc();

      //printf("# SatCheck() %ld, %ld..\n",
      //state->proc_non_trivial_count,
      //ProofStateCardinality(state));

      SatClauseSetImportProofState(set, state,
                                   control->heuristic_parms.sat_check_grounding,
                                   control->heuristic_parms.sat_check_normconst);

      // printf("# SatCheck()..imported\n");

      res = SatClauseSetCheckUnsat(set, &empty);
      state->satcheck_count++;
      if(res == PRUnsatisfiable)
      {
         state->satcheck_success++;
         state->satcheck_full_size = SatClauseSetCardinality(set);
         state->satcheck_actual_size = SatClauseSetNonPureCardinality(set);
         state->satcheck_core_size = SatClauseSetCoreSize(set);
      }
      else if(res == PRSatisfiable)
      {
         state->satcheck_satisfiable++;
      }
      SatClauseSetFree(set);
   }
   return empty;
}

#ifdef PRINT_SHARING

/*-----------------------------------------------------------------------
//
// Function: print_sharing_factor()
//
//   Determine the sharing factor and print it. Potentially expensive,
//   only useful for manual analysis.
//
// Global Variables:
//
// Side Effects    :
//
/----------------------------------------------------------------------*/

static void print_sharing_factor(ProofState_p state)
{
   long shared, unshared;
   shared = TBTermNodes(state->terms);
   unshared = ClauseSetGetTermNodes(state->tmp_store)+
      ClauseSetGetTermNodes(state->processed_pos_rules)+
      ClauseSetGetTermNodes(state->processed_pos_eqns)+
      ClauseSetGetTermNodes(state->processed_neg_units)+
      ClauseSetGetTermNodes(state->processed_non_units)+
      ClauseSetGetTermNodes(state->unprocessed);

   fprintf(GlobalOut, "\n#        GClauses   NClauses    NShared  "
           "NUnshared    TShared  TUnshared TSharinF   \n");
   fprintf(GlobalOut, "# grep %10ld %10ld %10ld %10ld %10ld %10ld %10.3f\n",
           state->generated_count - state->backward_rewritten_count,
           state->tmp_store->members,
           ClauseSetGetSharedTermNodes(state->tmp_store),
           ClauseSetGetTermNodes(state->tmp_store),
           shared,
           unshared,
           (float)unshared/shared);
}
#endif


#ifdef PRINT_RW_STATE

/*-----------------------------------------------------------------------
//
// Function: print_rw_state()
//
//   Print the system (R,E,NEW), e.g. the two types of demodulators
//   and the newly generated clauses.
//
// Global Variables: -
//
// Side Effects    : Output
//
/----------------------------------------------------------------------*/

void print_rw_state(ProofState_p state)
{
   char prefix[20];

   sprintf(prefix, "RWD%09ld_R: ",state->proc_non_trivial_count);
   ClauseSetPrintPrefix(GlobalOut,prefix,state->processed_pos_rules);
   sprintf(prefix, "RWD%09ld_E: ",state->proc_non_trivial_count);
   ClauseSetPrintPrefix(GlobalOut,prefix,state->processed_pos_eqns);
   sprintf(prefix, "RWD%09ld_N: ",state->proc_non_trivial_count);
   ClauseSetPrintPrefix(GlobalOut,prefix,state->tmp_store);
}

#endif





/*---------------------------------------------------------------------*/
/*                         Exported Functions                          */
/*---------------------------------------------------------------------*/



/*-----------------------------------------------------------------------
//
// Function: ProofControlInit()
//
//   Initialize a proof control cell for a given proof state (with at
//   least axioms and signature) and a set of parameters
//   describing the ordering and heuristics.
//
// Global Variables: -
//
// Side Effects    : Sets specs.
//
/----------------------------------------------------------------------*/

void ProofControlInit(ProofState_p state,ProofControl_p control,
                      HeuristicParms_p params, FVIndexParms_p fvi_params,
                      PStack_p wfcb_defs, PStack_p hcb_defs)
{
   PStackPointer sp;
   Scanner_p in;

   assert(control && control->wfcbs);
   assert(state && state->axioms && state->signature);
   assert(params);
   assert(!control->ocb);
   assert(!control->hcb);

   SpecFeaturesCompute(&(control->problem_specs),
                       state->axioms,state->signature);

   control->ocb = TOSelectOrdering(state, params,
                                   &(control->problem_specs));

   in = CreateScanner(StreamTypeInternalString,
                      DefaultWeightFunctions,
                      true, NULL);
   WeightFunDefListParse(control->wfcbs, in, control->ocb, state);
   DestroyScanner(in);

   for(sp = 0; sp < PStackGetSP(wfcb_defs); sp++)
   {
      in = CreateScanner(StreamTypeOptionString,
                         PStackElementP(wfcb_defs, sp) ,
                         true, NULL);
      WeightFunDefListParse(control->wfcbs, in, control->ocb, state);
      DestroyScanner(in);
   }
   in = CreateScanner(StreamTypeInternalString,
                      DefaultHeuristics,
                      true, NULL);
   HeuristicDefListParse(control->hcbs, in, control->wfcbs,
                         control->ocb, state);
   DestroyScanner(in);
   for(sp = 0; sp < PStackGetSP(hcb_defs); sp++)
   {
      in = CreateScanner(StreamTypeOptionString,
                         PStackElementP(hcb_defs, sp) ,
                         true, NULL);
      HeuristicDefListParse(control->hcbs, in, control->wfcbs,
                            control->ocb, state);
      DestroyScanner(in);
   }
   control->heuristic_parms     = *params;

   control->hcb = GetHeuristic(params->heuristic_name,
                               state,
                               control,
                               params);
   control->fvi_parms           = *fvi_params;
   if(!control->heuristic_parms.split_clauses)
   {
      control->fvi_parms.symbol_slack = 0;
   }
}


/*-----------------------------------------------------------------------
//
// Function: ProofStateResetProcessedSet()
//
//   Move all clauses from set into state->unprocessed.
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/

void ProofStateResetProcessedSet(ProofState_p state,
                                 ProofControl_p control,
                                 ClauseSet_p set)
{
   Clause_p handle;
   Clause_p tmpclause;

   while((handle = ClauseSetExtractFirst(set)))
   {
      if(ClauseQueryProp(handle, CPIsGlobalIndexed))
      {
         GlobalIndicesDeleteClause(&(state->gindices), handle);
      }

      ClauseKillChildren(handle); /* Should be none, but better be safe */

      if(ProofObjectRecordsGCSelection)
      {
         ClausePushDerivation(handle, DCCnfEvalGC, NULL, NULL);
      }
      tmpclause = ClauseFlatCopy(handle);
      ClausePushDerivation(tmpclause, DCCnfQuote, handle, NULL);
      ClauseSetInsert(state->archive, handle);
      handle = tmpclause;
      HCBClauseEvaluate(control->hcb, handle);
      ClauseDelProp(handle, CPIsOriented);
      DocClauseQuoteDefault(6, handle, "move_eval");

      if(control->heuristic_parms.prefer_initial_clauses)
      {
         EvalListChangePriority(handle->evaluations, -PrioLargestReasonable);
      }
      ClauseSetInsert(state->unprocessed, handle);
   }
}


/*-----------------------------------------------------------------------
//
// Function: ProofStateResetProcessed()
//
//   Move all clauses from the processed clause sets to unprocessed.
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/

void ProofStateResetProcessed(ProofState_p state, ProofControl_p control)
{
   ProofStateResetProcessedSet(state, control, state->processed_pos_rules);
   ProofStateResetProcessedSet(state, control, state->processed_pos_eqns);
   ProofStateResetProcessedSet(state, control, state->processed_neg_units);
   ProofStateResetProcessedSet(state, control, state->processed_non_units);
}


/*-----------------------------------------------------------------------
//
// Function: fvi_param_init()
//
//   Initialize the parameters for all feature vector indices in
//   state.
//
// Global Variables: -
//
// Side Effects    : -
//
/----------------------------------------------------------------------*/

void fvi_param_init(ProofState_p state, ProofControl_p control)
{
   long symbols;
   PermVector_p perm;
   FVCollect_p  cspec;

   state->fvi_initialized = true;
   state->original_symbols = state->signature->f_count;

   symbols = MIN(state->original_symbols+control->fvi_parms.symbol_slack,
                 control->fvi_parms.max_symbols);

   // printf("### Symbols: %ld\n", symbols);
   switch(control->fvi_parms.cspec.features)
   {
   case FVIBillFeatures:
         cspec = BillFeaturesCollectAlloc(state->signature, symbols*2+2);
         break;
   case FVIBillPlusFeatures:
         cspec = BillPlusFeaturesCollectAlloc(state->signature, symbols*2+4);
         break;
   case FVIACFold:
         // printf("# FVIACFold\n");
         cspec = FVCollectAlloc(FVICollectFeatures,
                                true,
                                0,
                                symbols*2+2,
                                2,
                                0,
                                symbols,
                                symbols+2,
                                0,
                                symbols,
                                0,0,0,
                                0,0,0);
         break;
   case FVIACStagger:
         cspec = FVCollectAlloc(FVICollectFeatures,
                                true,
                                0,
                                symbols*2+2,
                                2,
                                0,
                                2*symbols,
                                2,
                                2+symbols,
                                2*symbols,
                                0,0,0,
                                0,0,0);
         break;
   case FVICollectFeatures:
         cspec = FVCollectAlloc(control->fvi_parms.cspec.features,
                                control->fvi_parms.cspec.use_litcount,
                                control->fvi_parms.cspec.ass_vec_len,
                                symbols,
                                control->fvi_parms.cspec.pos_count_base,
                                control->fvi_parms.cspec.pos_count_offset,
                                control->fvi_parms.cspec.pos_count_mod,
                                control->fvi_parms.cspec.neg_count_base,
                                control->fvi_parms.cspec.neg_count_offset,
                                control->fvi_parms.cspec.neg_count_mod,
                                control->fvi_parms.cspec.pos_depth_base,
                                control->fvi_parms.cspec.pos_depth_offset,
                                control->fvi_parms.cspec.pos_depth_mod,
                                control->fvi_parms.cspec.neg_depth_base,
                                control->fvi_parms.cspec.neg_depth_offset,
                                control->fvi_parms.cspec.neg_depth_mod);

         break;
   default:
         cspec = FVCollectAlloc(control->fvi_parms.cspec.features,
                                0,
                                0,
                                0,
                                0, 0, 0,
                                0, 0, 0,
                                0, 0, 0,
                                0, 0, 0);
         break;
   }
   cspec->max_symbols=symbols;
   state->fvi_cspec = cspec;

   perm = PermVectorCompute(state->axioms,
                            cspec,
                            control->fvi_parms.eliminate_uninformative);
   if(control->fvi_parms.cspec.features != FVINoFeatures)
   {
      state->processed_non_units->fvindex =
         FVIAnchorAlloc(cspec, PermVectorCopy(perm));
      state->processed_pos_rules->fvindex =
         FVIAnchorAlloc(cspec, PermVectorCopy(perm));
      state->processed_pos_eqns->fvindex =
         FVIAnchorAlloc(cspec, PermVectorCopy(perm));
      state->processed_neg_units->fvindex =
         FVIAnchorAlloc(cspec, PermVectorCopy(perm));
      if(state->watchlist)
      {
         state->watchlist->fvindex =
            FVIAnchorAlloc(cspec, PermVectorCopy(perm));
         //ClauseSetNewTerms(state->watchlist, state->terms);
      }
   }
   state->def_store_cspec = FVCollectAlloc(FVICollectFeatures,
                                           true,
                                           0,
                                           symbols*2+2,
                                           2,
                                           0,
                                           symbols,
                                           symbols+2,
                                           0,
                                           symbols,
                                           0,0,0,
                                           0,0,0);
   state->definition_store->def_clauses->fvindex =
      FVIAnchorAlloc(state->def_store_cspec, perm);
}



/*-----------------------------------------------------------------------
//
// Function: ProofStateInit()
//
//   Given a proof state with axioms and a heuristic parameter
//   description, initialize the ProofStateCell, i.e. generate the
//   HCB, the ordering, and evaluate the axioms and put them in the
//   unprocessed list.
//
// Global Variables:
//
// Side Effects    :
//
/----------------------------------------------------------------------*/

void ProofStateInit(ProofState_p state, ProofControl_p control)
{
   Clause_p handle, new;
   HCB_p    tmphcb;
   PStack_p traverse;
   Eval_p   cell;

   OUTPRINT(1, "# Initializing proof state\n");

   assert(ClauseSetEmpty(state->processed_pos_rules));
   assert(ClauseSetEmpty(state->processed_pos_eqns));
   assert(ClauseSetEmpty(state->processed_neg_units));
   assert(ClauseSetEmpty(state->processed_non_units));

   if(!state->fvi_initialized)
   {
      fvi_param_init(state, control);
   }
   ProofStateInitWatchlist(state, control->ocb);

   tmphcb = GetHeuristic("Uniq", state, control, &(control->heuristic_parms));
   assert(tmphcb);
   ClauseSetReweight(tmphcb, state->axioms);

   traverse =
      EvalTreeTraverseInit(PDArrayElementP(state->axioms->eval_indices,0),0);

   while((cell = EvalTreeTraverseNext(traverse, 0)))
   {
      handle = cell->object;
      new = ClauseCopy(handle, state->terms);

      ClauseSetProp(new, CPInitial);
      if(state->watchlist)
      {
         check_watchlist(&(state->wlindices), state->watchlist,
                         new, state->archive,
                         control->heuristic_parms.watchlist_is_static);
      }
      HCBClauseEvaluate(control->hcb, new);
      DocClauseQuoteDefault(6, new, "eval");
      ClausePushDerivation(new, DCCnfQuote, handle, NULL);
      if(ProofObjectRecordsGCSelection)
      {
         ClausePushDerivation(new, DCCnfEvalGC, NULL, NULL);
      }
      if(control->heuristic_parms.prefer_initial_clauses)
      {
         EvalListChangePriority(new->evaluations, -PrioLargestReasonable);
      }
      ClauseSetInsert(state->unprocessed, new);
   }
   ClauseSetMarkSOS(state->unprocessed, control->heuristic_parms.use_tptp_sos);
   // printf("Before EvalTreeTraverseExit\n");
   EvalTreeTraverseExit(traverse);

   if(control->heuristic_parms.ac_handling!=NoACHandling)
   {
      if(OutputLevel)
      {
         fprintf(GlobalOut, "# Scanning for AC axioms\n");
      }
      control->ac_handling_active = ClauseSetScanAC(state->signature,
                                                    state->unprocessed);
      if(OutputLevel)
      {
         SigPrintACStatus(GlobalOut, state->signature);
         if(control->ac_handling_active)
         {
            fprintf(GlobalOut, "# AC handling enabled\n");
         }
      }
   }

   GlobalIndicesInit(&(state->gindices),
                     state->signature,
                     control->heuristic_parms.rw_bw_index_type,
                     control->heuristic_parms.pm_from_index_type,
                     control->heuristic_parms.pm_into_index_type);

}


/*-----------------------------------------------------------------------
//
// Function: ProcessClause()
//
//   Select an unprocessed clause, process it. Return pointer to empty
//   clause if it can be derived, NULL otherwise. This is the core of
//   the main proof procedure.
//
// Global Variables: -
//
// Side Effects    : Everything ;-)
//
/----------------------------------------------------------------------*/

Clause_p ProcessClause(ProofState_p state, ProofControl_p control,
                       long answer_limit)
{
   Clause_p         clause, resclause, tmp_copy, empty, arch_copy = NULL;
   FVPackedClause_p pclause;
   SysDate          clausedate;

   clause = control->hcb->hcb_select(control->hcb,
                                     state->unprocessed);
   //EvalListPrintComment(GlobalOut, clause->evaluations); printf("\n");
   if(OutputLevel==1)
   {
      putc('#', GlobalOut);
   }
   assert(clause);

   state->processed_count++;

   ClauseSetExtractEntry(clause);
   ClauseSetProp(clause, CPIsProcessed);
   ClauseDetachParents(clause);
   ClauseRemoveEvaluations(clause);

   assert(!ClauseQueryProp(clause, CPIsIRVictim));

   if(ProofObjectRecordsGCSelection)
   {
      arch_copy = ClauseArchive(state->archive, clause);
   }

   if(!(pclause = ForwardContractClause(state, control,
                                        clause, true,
                                        control->heuristic_parms.forward_context_sr,
                                        control->heuristic_parms.condensing,
                                        FullRewrite)))
   {
      if(arch_copy)
      {
         ClauseSetDeleteEntry(arch_copy);
      }
      return NULL;
   }

   if(ClauseIsSemFalse(pclause->clause))
   {
      state->answer_count ++;
      ClausePrintAnswer(GlobalOut, pclause->clause, state);
      PStackPushP(state->extract_roots, pclause->clause);
      if(ClauseIsEmpty(pclause->clause)||
         state->answer_count>=answer_limit)
      {
         clause = FVUnpackClause(pclause);
         ClauseEvaluateAnswerLits(clause);
         return clause;
      }
   }
   assert(ClauseIsSubsumeOrdered(pclause->clause));
   check_ac_status(state, control, pclause->clause);

   document_processing(pclause->clause);
   state->proc_non_trivial_count++;

   resclause = replacing_inferences(state, control, pclause);
   if(!resclause || ClauseIsEmpty(resclause))
   {
      if(resclause)
      {
         PStackPushP(state->extract_roots, resclause);
      }
      return resclause;
   }

   if(state->watchlist)
   {
      check_watchlist(&(state->wlindices), state->watchlist,
                      pclause->clause, state->archive,
                      control->heuristic_parms.watchlist_is_static);
   }

   /* Now on to backward simplification. */
   clausedate = ClauseSetListGetMaxDate(state->demods, FullRewrite);

   eliminate_backward_rewritten_clauses(state, control, pclause->clause, &clausedate);
   eliminate_backward_subsumed_clauses(state, pclause);
   eliminate_unit_simplified_clauses(state, pclause->clause);
   eliminate_context_sr_clauses(state, control, pclause->clause);
   ClauseSetSetProp(state->tmp_store, CPIsIRVictim);

   clause = pclause->clause;

   ClauseNormalizeVars(clause, state->freshvars);
   tmp_copy = ClauseCopyDisjoint(clause);
   tmp_copy->ident = clause->ident;

   clause->date = clausedate;
   ClauseSetProp(clause, CPLimitedRW);

   if(ClauseIsDemodulator(clause))
   {
      assert(clause->neg_lit_no == 0);
      if(EqnIsOriented(clause->literals))
      {
         TermCellSetProp(clause->literals->lterm, TPIsRewritable);
         state->processed_pos_rules->date = clausedate;
         ClauseSetIndexedInsert(state->processed_pos_rules, pclause);
      }
      else
      {
         state->processed_pos_eqns->date = clausedate;
         ClauseSetIndexedInsert(state->processed_pos_eqns, pclause);
      }
   }
   else if(ClauseLiteralNumber(clause) == 1)
   {
      assert(clause->neg_lit_no == 1);
      ClauseSetIndexedInsert(state->processed_neg_units, pclause);
   }
   else
   {
      ClauseSetIndexedInsert(state->processed_non_units, pclause);
   }
   GlobalIndicesInsertClause(&(state->gindices), clause);

   FVUnpackClause(pclause);
   ENSURE_NULL(pclause);
   if(state->watchlist && control->heuristic_parms.watchlist_simplify)
   {
      simplify_watchlist(state, control, clause);
   }
   if(control->heuristic_parms.selection_strategy != SelectNoGeneration)
   {
      generate_new_clauses(state, control, clause, tmp_copy);
   }
   ClauseFree(tmp_copy);
   if(TermCellStoreNodes(&(state->tmp_terms->term_store))>TMPBANK_GC_LIMIT)
   {
      TBGCSweep(state->tmp_terms);
   }
#ifdef PRINT_SHARING
   print_sharing_factor(state);
#endif
#ifdef PRINT_RW_STATE
   print_rw_state(state);
#endif
   if(control->heuristic_parms.detsort_tmpset)
   {
      ClauseSetSort(state->tmp_store, ClauseCmpByStructWeight);
   }
   if((empty = insert_new_clauses(state, control)))
   {
	  printf("empty = insert_new_clauses, inserting new clauses\n"); //////////////////////////////////////////////////////////////
      PStackPushP(state->extract_roots, empty);
      return empty;
   }
   return NULL;
}


/*-----------------------------------------------------------------------
//
// Function:  Saturate()
//
//   Process clauses until either the empty clause has been derived, a
//   specified number of clauses has been processed, or the clause set
//   is saturated. Return empty clause (if found) or NULL.
//
// Global Variables: -
//
// Side Effects    : Modifies state.
//
/----------------------------------------------------------------------*/

Clause_p Saturate(ProofState_p state, ProofControl_p control, long
                  step_limit, long proc_limit, long unproc_limit, long
                  total_limit, long generated_limit, long tb_insert_limit,
                  long answer_limit)
{
   Clause_p unsatisfiable = NULL;
   long
      count = 0,
      sat_check_size_limit = control->heuristic_parms.sat_check_size_limit,
      sat_check_step_limit = control->heuristic_parms.sat_check_step_limit,
      sat_check_ttinsert_limit = control->heuristic_parms.sat_check_ttinsert_limit;


   while(!TimeIsUp &&
         !ClauseSetEmpty(state->unprocessed) &&
         step_limit   > count &&
         proc_limit   > ProofStateProcCardinality(state) &&
         unproc_limit > ProofStateUnprocCardinality(state) &&
         total_limit  > ProofStateCardinality(state) &&
         generated_limit > (state->generated_count -
                            state->backward_rewritten_count)&&
         tb_insert_limit > state->terms->insertions &&
         (!state->watchlist||!ClauseSetEmpty(state->watchlist)))
   {
      count++;
      printf("processing new clause!\n");
      unsatisfiable = ProcessClause(state, control, answer_limit);
      if(unsatisfiable)
      {
         break;
      }
      unsatisfiable = cleanup_unprocessed_clauses(state, control);
      if(unsatisfiable)
      {
         break;
      }
      if(control->heuristic_parms.sat_check_grounding != GMNoGrounding)
      {
         if(ProofStateCardinality(state) >= sat_check_size_limit)
         {
            unsatisfiable = SATCheck(state, control);
            while(sat_check_size_limit <= ProofStateCardinality(state))
            {
               sat_check_size_limit += control->heuristic_parms.sat_check_size_limit;
            }
         }
         else if(state->proc_non_trivial_count >= sat_check_step_limit)
         {
            unsatisfiable = SATCheck(state, control);
            sat_check_step_limit += control->heuristic_parms.sat_check_step_limit;
         }
         else if( state->terms->insertions >= sat_check_ttinsert_limit)
         {
            unsatisfiable = SATCheck(state, control);
            sat_check_ttinsert_limit *=2;
         }
         if(unsatisfiable)
         {
            PStackPushP(state->extract_roots, unsatisfiable);
            break;
         }
      }
   }
   return unsatisfiable;
}


/*---------------------------------------------------------------------*/
/*                        End of File                                  */
/*---------------------------------------------------------------------*/

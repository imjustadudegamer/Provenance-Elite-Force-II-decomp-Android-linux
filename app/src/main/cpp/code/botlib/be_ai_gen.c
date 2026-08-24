

#include "../qcommon/q_shared.h"
#include "l_memory.h"
#include "l_log.h"
#include "l_utils.h"
#include "l_script.h"
#include "l_precomp.h"
#include "l_struct.h"
#include "aasfile.h"
#include "botlib.h"
#include "be_aas.h"
#include "be_aas_funcs.h"
#include "be_interface.h"
#include "be_ai_gen.h"

int GeneticSelection(int numranks, float *rankings)
{
	float sum;
	int i, index;

	sum = 0;
	for (i = 0; i < numranks; i++)
	{
		if (rankings[i] < 0) continue;
		sum += rankings[i];
	} 
	if (sum > 0)
	{
		for (i = 0; i < numranks; i++)
		{
			if (rankings[i] < 0) continue;
			sum -= rankings[i];
			if (sum <= 0) return i;
		} 
	} 
	index = random() * numranks;
	for (i = 0; i < numranks; i++)
	{
		if (rankings[index] >= 0) return index;
		index = (index + 1) % numranks;
	} 
	return 0;
} 
int GeneticParentsAndChildSelection(int numranks, float *ranks, int *parent1, int *parent2, int *child)
{
	float rankings[256], max;
	int i;

	if (numranks > 256)
	{
		botimport.Print(PRT_WARNING, "GeneticParentsAndChildSelection: too many bots\n");
		*parent1 = *parent2 = *child = 0;
		return qfalse;
	} 
	for (max = 0, i = 0; i < numranks; i++)
	{
		if (ranks[i] < 0) continue;
		max++;
	} 
	if (max < 3)
	{
		botimport.Print(PRT_WARNING, "GeneticParentsAndChildSelection: too few valid bots\n");
		*parent1 = *parent2 = *child = 0;
		return qfalse;
	} 
	Com_Memcpy(rankings, ranks, sizeof(float) * numranks);
	*parent1 = GeneticSelection(numranks, rankings);
	rankings[*parent1] = -1;
	*parent2 = GeneticSelection(numranks, rankings);
	rankings[*parent2] = -1;
	max = 0;
	for (i = 0; i < numranks; i++)
	{
		if (rankings[i] < 0) continue;
		if (rankings[i] > max) max = rankings[i];
	} 
	for (i = 0; i < numranks; i++)
	{
		if (rankings[i] < 0) continue;
		rankings[i] = max - rankings[i];
	} 
	*child = GeneticSelection(numranks, rankings);
	return qtrue;
} 

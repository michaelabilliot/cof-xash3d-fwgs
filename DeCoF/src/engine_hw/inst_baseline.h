// inst_baseline.h -- instanced baselines (sv_main.c)

#ifndef INST_BASELINE_H
#define INST_BASELINE_H

#define NUM_BASELINES	64

typedef struct extra_baselines_s
{
	int				number;
	int				classname[NUM_BASELINES];
	entity_state_t	baseline[NUM_BASELINES];
} extra_baselines_t;

#endif // INST_BASELINE_H

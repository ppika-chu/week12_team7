#pragma once

#include "LightweightStats.h"

namespace ParticleStats
{
DECLARE_COUNTER_STAT("Alive Particles", STAT_AliveParticles);
DECLARE_COUNTER_STAT("Particle Draw Calls", STAT_DrawCalls);
DECLARE_CYCLE_STAT("Particle Tick CPU", STAT_TickCPU);
}
#pragma once

#include "bio/genome.hpp"
#include "core/rng.hpp"
#include "sim/body_particles.hpp"

#include <cstdint>

struct RingBody;

// Tipo visual de las partículas interiores (BodyParticles::tag).
enum class OrganelleTag : std::uint8_t { Granule = 0, Mitochondrion, Chloroplast, Lysosome };

// Agrega organelos hasta la densidad que pide el genoma para el área actual de la célula
// (los organelos se replican al crecer; tras una división cada hija completa los suyos).
void refillOrganelles(RingBody& body, BodyParticles& bp, const Genome& genome, Rng& rng);

int countOrganelles(const RingBody& body, const BodyParticles& bp, OrganelleTag tag);

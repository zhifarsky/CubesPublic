#pragma once
#include <glm.hpp>
#include "Typedefs.h"
enum EntityType : u8 {
	entityNull,
	entityZombie,
	entityChiken,
	entityCOUNT
};

enum EntityState : u8 {
	enitityStateNull,
	entityStateIdle,
	entityStateChasing,
	entityStateCOUNT
};

struct Entity {
	glm::vec3 pos;
	glm::vec3 rot = { 0,90,0 };
	glm::vec3 speed = {0,0,0};
	EntityType type;
	EntityState state;

	void jump();
};


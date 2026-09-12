#pragma once

class CCollisionProperty
{
public:
	SCHEMA_FIELD(math::vector3, m_vecMins, "client.dll", "CCollisionProperty", "m_vecMins")
	SCHEMA_FIELD(math::vector3, m_vecMaxs, "client.dll", "CCollisionProperty", "m_vecMaxs")
};

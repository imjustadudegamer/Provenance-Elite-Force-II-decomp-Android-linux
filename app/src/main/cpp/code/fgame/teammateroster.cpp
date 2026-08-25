

#include "g_local.h"
#include "teammateroster.hpp"
#include "entity.h"
#include "../qcommon/gameplaymanager.h"
#include "archive.h"

TeammateRoster *TeammateRoster::_instance = NULL;

TeammateRoster::TeammateRoster()
{
    GameplayManager *gpm = GameplayManager::getTheGameplayManager();

    if (!gpm) {
        return;
    }

    _healthyShader  = gpm->getStringValue("TeammateData", "HealthyShader");
    _injuredShader  = gpm->getStringValue("TeammateData", "InjuredShader");
    _criticalShader = gpm->getStringValue("TeammateData", "CriticalShader");
    _defaultShader  = gpm->getStringValue("TeammateData", "DefaultShader");

    _healthyAnimation  = gpm->getStringValue("TeammateData", "HealthyAnimation");
    _injuredAnimation  = gpm->getStringValue("TeammateData", "InjuredAnimation");
    _criticalAnimation = gpm->getStringValue("TeammateData", "CriticalAnimation");
}

TeammateRoster::~TeammateRoster()
{
    int i;

    for (i = 0; i < MAX_TEAMMATES; i++) {
        removeTeammate(i);
    }
}

void TeammateRoster::createInstance(void)
{
    if (_instance == NULL) {
        _instance = new TeammateRoster;
    }
}

void TeammateRoster::deleteInstance(void)
{
    delete _instance;
    _instance = NULL;
}

void TeammateRoster::addTeammate(const Entity *entity, int index)
{
    if (index > MAX_TEAMMATES - 1 || index < 0) {
        return;
    }

    if (entity == NULL) {
        return;
    }

    TeammateData& teammateData = _teammateList[index];

    teammateData.entNum = entity->edict->s.number;
    teammateData.injuredHealthLevel  = (int)(entity->max_health - (entity->max_health * 0.33f));
    teammateData.criticalHealthLevel = (int)(entity->max_health - (entity->max_health * 0.66f));

    teammateData.archeType = ((Entity *)entity)->getArchetype();
    teammateData.active    = true;
}

void TeammateRoster::removeTeammate(int index)
{
    if (index > MAX_TEAMMATES - 1 || index < 0) {
        return;
    }

    TeammateData& teammateData = _teammateList[index];

    teammateData.entNum              = ENTITYNUM_NONE;
    teammateData.criticalHealthLevel = 0;
    teammateData.injuredHealthLevel  = 0;
    teammateData.active              = false;

    removeTeammateDataFromDatabase(index);
}

void TeammateRoster::removeTeammateDataFromDatabase(int index)
{
    GameplayManager *gpm = GameplayManager::getTheGameplayManager();
    str              objectName;

    if (!gpm) {
        return;
    }

    objectName = "Teammate";
    objectName += (index + 1);

    gpm->setStringValue(objectName.c_str(), "Archetype", "EMPTY");
    gpm->setStringValue(objectName.c_str(), "StatusShader", _defaultShader.c_str());
}

TeammateStatus TeammateRoster::getTeammateStatus(int index)
{
    if (index > MAX_TEAMMATES - 1 || index < 0) {
        return HEALTHY_STATUS;
    }

    TeammateData& teammateData = _teammateList[index];

    if (teammateData.entNum == ENTITYNUM_NONE || teammateData.entNum < 0
        || teammateData.entNum >= globals.max_entities) {
        return HEALTHY_STATUS;
    }

    gentity_t *gentity = &g_entities[teammateData.entNum];

    if (gentity->entity == NULL) {
        return HEALTHY_STATUS;
    }

    if (gentity->entity->health > teammateData.injuredHealthLevel) {
        return HEALTHY_STATUS;
    } else if (gentity->entity->health <= teammateData.injuredHealthLevel
               && gentity->entity->health > teammateData.criticalHealthLevel) {
        return INJURED_STATUS;
    } else if (gentity->entity->health <= teammateData.criticalHealthLevel) {
        return CRITICAL_STATUS;
    }

    return HEALTHY_STATUS;
}

void TeammateRoster::update(void)
{
    GameplayManager *gpm = GameplayManager::getTheGameplayManager();
    int              i;

    if (!gpm) {
        return;
    }

    for (i = 0; i < MAX_TEAMMATES; i++) {
        str            objectName;
        str            objectRenderName;
        str            shader;
        str            animation;
        TeammateStatus status;

        if (_teammateList[i].active == false) {
            continue;
        }

        objectName = "Teammate";
        objectName += (i + 1);

        status = getTeammateStatus(i);

        switch (status) {
        case INJURED_STATUS:
            shader    = _injuredShader;
            animation = _injuredAnimation;
            break;

        case CRITICAL_STATUS:
            shader    = _criticalShader;
            animation = _criticalAnimation;
            break;

        case HEALTHY_STATUS:
        default:
            shader    = _healthyShader;
            animation = _healthyAnimation;
            break;
        }

        gpm->setStringValue(objectName.c_str(), "Archetype", _teammateList[i].archeType.c_str(), qtrue);

        objectRenderName = _teammateList[i].archeType;
        objectRenderName += ".ModelRendering";
        gpm->setStringValue(objectRenderName.c_str(), "animation", animation.c_str(), qtrue);

        gpm->setStringValue(objectName.c_str(), "StatusShader", shader.c_str(), qtrue);
    }
}

void TeammateRoster::clearTeammates(void)
{
    int i;

    for (i = 0; i < MAX_TEAMMATES; i++) {
        removeTeammate(i);
    }
}


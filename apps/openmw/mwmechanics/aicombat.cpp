#include "aicombat.hpp"

#include "movement.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/timestamp.hpp"
#include "../mwbase/world.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/dialoguemanager.hpp"

#include "creaturestats.hpp"
#include "npcstats.hpp"

#include <OgreMath.h>

namespace
{
    static float sgn(Ogre::Radian a)
    {
        if(a.valueDegrees() > 0)
            return 1.0;
        return -1.0;
    }
}

namespace MWMechanics
{

    AiCombat::AiCombat(const std::string &targetId)
        :mTargetId(targetId),mTimer(0),mTimer2(0)
    {
    }

    bool AiCombat::execute (const MWWorld::Ptr& actor,float duration)
    {
        if(!MWWorld::Class::get(actor).getCreatureStats(actor).isHostile()) return true;

        const MWWorld::Ptr target = MWBase::Environment::get().getWorld()->getPtr(mTargetId, false);

        if(MWWorld::Class::get(actor).getCreatureStats(actor).getHealth().getCurrent() <= 0) return true;

        actor.getClass().getCreatureStats(actor).setMovementFlag(CreatureStats::Flag_Run, true);

        if(actor.getTypeName() == typeid(ESM::NPC).name())
        {
            MWMechanics::DrawState_ state = actor.getClass().getNpcStats(actor).getDrawState();
            if (state == MWMechanics::DrawState_Spell || state == MWMechanics::DrawState_Nothing)
                actor.getClass().getNpcStats(actor).setDrawState(MWMechanics::DrawState_Weapon);
            //MWWorld::Class::get(actor).getCreatureStats(actor).setAttackingOrSpell(true);
        }
        ESM::Position pos = actor.getRefData().getPosition();
        const ESM::Pathgrid *pathgrid =
            MWBase::Environment::get().getWorld()->getStore().get<ESM::Pathgrid>().search(*actor.getCell()->mCell);

        float xCell = 0;
        float yCell = 0;

        if (actor.getCell()->mCell->isExterior())
        {
            xCell = actor.getCell()->mCell->mData.mX * ESM::Land::REAL_SIZE;
            yCell = actor.getCell()->mCell->mData.mY * ESM::Land::REAL_SIZE;
        }

        ESM::Pathgrid::Point dest;
        dest.mX = target.getRefData().getPosition().pos[0];
        dest.mY = target.getRefData().getPosition().pos[1];
        dest.mZ = target.getRefData().getPosition().pos[2];

        ESM::Pathgrid::Point start;
        start.mX = pos.pos[0];
        start.mY = pos.pos[1];
        start.mZ = pos.pos[2];

        mTimer2 = mTimer2 + duration;

        if(!mPathFinder.isPathConstructed())
            mPathFinder.buildPath(start, dest, pathgrid, xCell, yCell, true);
        else
        {
            mPathFinder2.buildPath(start, dest, pathgrid, xCell, yCell, true);
            ESM::Pathgrid::Point lastPt = mPathFinder.getPath().back();
            if((mTimer2 > 0.25)&&(mPathFinder2.getPathSize() < mPathFinder.getPathSize() ||
                (dest.mX - lastPt.mX)*(dest.mX - lastPt.mX)+(dest.mY - lastPt.mY)*(dest.mY - lastPt.mY)+(dest.mZ - lastPt.mZ)*(dest.mZ - lastPt.mZ) > 200*200))
            {
                mTimer2 = 0;
                mPathFinder = mPathFinder2;
            }
        }

        mPathFinder.checkPathCompleted(pos.pos[0],pos.pos[1],pos.pos[2]);

        float zAngle = mPathFinder.getZAngleToNext(pos.pos[0], pos.pos[1]);

        // TODO: use movement settings instead of rotating directly
        MWBase::Environment::get().getWorld()->rotateObject(actor, 0, 0, zAngle, false);
        MWWorld::Class::get(actor).getMovementSettings(actor).mPosition[1] = 1;


        float range = 100;
        MWWorld::Class::get(actor).getCreatureStats(actor).setAttackingOrSpell(false);
        if((dest.mX - start.mX)*(dest.mX - start.mX)+(dest.mY - start.mY)*(dest.mY - start.mY)+(dest.mZ - start.mZ)*(dest.mZ - start.mZ)
            < range*range)
        {
            float directionX = dest.mX - start.mX;
            float directionY = dest.mY - start.mY;
            float directionResult = sqrt(directionX * directionX + directionY * directionY);

            zAngle = Ogre::Radian( Ogre::Math::ACos(directionY / directionResult) * sgn(Ogre::Math::ASin(directionX / directionResult)) ).valueDegrees();
            // TODO: use movement settings instead of rotating directly
            MWBase::Environment::get().getWorld()->rotateObject(actor, 0, 0, zAngle, false);

            mPathFinder.clearPath();

            if(mTimer == 0)
            {
                MWWorld::Class::get(actor).getCreatureStats(actor).setAttackingOrSpell(false);
                //mTimer = mTimer + duration;
            }
            if( mTimer > 1)
            {
                if (actor.getClass().isNpc())
                {
                    const MWWorld::ESMStore &store = MWBase::Environment::get().getWorld()->getStore();
                    int chance = store.get<ESM::GameSetting>().find("iVoiceAttackOdds")->getInt();
                    int roll = std::rand()/ (static_cast<double> (RAND_MAX) + 1) * 100; // [0, 99]
                    if (roll < chance)
                    {
                        MWBase::Environment::get().getDialogueManager()->say(actor, "attack");
                    }
                }

                MWWorld::Class::get(actor).getCreatureStats(actor).setAttackingOrSpell(true);
                mTimer = 0;
            }
            else
            {
                mTimer = mTimer + duration;
            }

            MWWorld::Class::get(actor).getMovementSettings(actor).mPosition[1] = 0;
            //MWWorld::Class::get(actor).getCreatureStats(actor).setAttackingOrSpell(!MWWorld::Class::get(actor).getCreatureStats(actor).getAttackingOrSpell());
        }
        return false;
    }

    int AiCombat::getTypeId() const
    {
        return TypeIdCombat;
    }

    unsigned int AiCombat::getPriority() const
    {
        return 1;
    }

    const std::string &AiCombat::getTargetId() const
    {
        return mTargetId;
    }

    AiCombat *MWMechanics::AiCombat::clone() const
    {
        return new AiCombat(*this);
    }
}


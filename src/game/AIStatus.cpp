#include "game/AIStatus.h"

#include "game/AIObject.h"

CAIObject* CAIStatus::s_paioAttacker;
CAIObject* CAIStatus::s_paioTarget;

CAIStatus::CAIStatus() {}

CAIStatus::~CAIStatus() {}

bool CAIStatus::MatchStatusList(aistatus_match* list, int count) {
    for (; count > 0; count--, list++) {
        int var = list->var;
        int op = list->op;
        int value = list->value;
        int current = 0;
        switch (var) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 8:
        case 13:
            current = ((int*)this)[var];
            break;
        case 6:
            current = s_paioAttacker->ComputeStatusVisibility(s_paioTarget);
            break;
        case 7:
            current = s_paioAttacker->ComputeStatusDistance(s_paioTarget);
            break;
        case 9:
            current = s_paioAttacker->ComputeStatusThreatValue(s_paioTarget);
            break;
        case 10:
            current = s_paioAttacker->ComputeStatusLOSValue(s_paioTarget);
            break;
        case 11:
            current = s_paioAttacker->ComputeStatusExponentialDistance(s_paioTarget);
            break;
        case 12:
            current = s_paioAttacker->ComputeStatusIsEnemy(s_paioTarget);
            break;
        }

        bool match;
        switch (op) {
        case 0:
            match = current == value;
            break;
        case 1:
            match = current != value;
            break;
        case 2:
            match = current < value;
            break;
        case 3:
            match = current > value;
            break;
        default:
            match = false;
            break;
        }
        if (!match) {
            return false;
        }
    }
    return true;
}

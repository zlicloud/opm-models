// -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// vi: set et ts=4 sw=4 sts=4:
/*
  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.

  Consult the COPYING file in the top-level source directory of this
  module for the precise wording of the license and the list of
  copyright holders.
*/
/*!
 * \file
 *
 * \copydoc Ewoms::BlackOilOnePhaseIndices
 */
#ifndef EWOMS_BLACK_OIL_ONE_PHASE_INDICES_HH
#define EWOMS_BLACK_OIL_ONE_PHASE_INDICES_HH

#include <cassert>

namespace Opm {

/*!
 * \ingroup BlackOilModel
 *
 * \brief The primary variable and equation indices for the black-oil model.
 */
template <unsigned numSolventsV, unsigned numExtbosV, unsigned numPolymersV, unsigned numEnergyV, bool enableFoam, bool enableBrine, unsigned PVOffset, unsigned canonicalCompIdx, unsigned numMICPsV>
struct BlackOilOnePhaseIndices
{
    //! Is phase enabled or not
    static const bool oilEnabled = (canonicalCompIdx == 0);
    static const bool waterEnabled = (canonicalCompIdx == 1);
    static const bool gasEnabled = (canonicalCompIdx == 2);

    //! Are solvents involved?
    static const bool enableSolvent = numSolventsV > 0;

    //! Is extbo invoked?
    static const bool enableExtbo = numExtbosV > 0;

    //! Are polymers involved?
    static const bool enablePolymer = numPolymersV > 0;

    //! Shall energy be conserved?
    static const bool enableEnergy = numEnergyV > 0;

    //! Is MICP involved?
    static const bool enableMICP = numMICPsV > 0;

    //! Number of solvent components to be considered
    static const int numSolvents = enableSolvent ? numSolventsV : 0;

    //! Number of components to be considered for extbo
    static const int numExtbos = enableExtbo ? numExtbosV : 0;

    //! Number of polymer components to be considered
    static const int numPolymers = enablePolymer ? numPolymersV : 0;

    //! Number of energy equations to be considered
    static const int numEnergy = enableEnergy ? numEnergyV : 0;

    //! Number of foam equations to be considered
    static const int numFoam = enableFoam? 1 : 0;

    //! Number of salt equations to be considered
    static const int numBrine = enableBrine? 1 : 0;

    //! The number of fluid phases
    static const int numPhases = 1;

    //! Number of MICP components to be considered
    static const int numMICPs = enableMICP ? numMICPsV : 0;

    //! The number of equations
    static const int numEq = numPhases + numSolvents + numExtbos + numPolymers + numEnergy + numFoam + numBrine + numMICPs;

    //////////////////////////////
    // Primary variable indices
    //////////////////////////////

    /*!
     * \brief Index of the switching variable which determines the composistion of the water phase
     *
     * Depending on the phases present, this variable is either interpreted as
     * water saturation or vapporized water in gas phase.
     *
     * \note For one-phase models this is disabled.
     */
    static const int waterSwitchIdx  = -10000;

    /*!
     * \brief Index of the switching variable which determines the pressure
     *
     * Depending on the phases present, this variable is either interpreted as the
     * pressure of the oil phase, gas phase (if no oil) or water phase (if only water)
     */
     static const int pressureSwitchIdx  = PVOffset + 0;

    /*!
     * \brief Index of the switching variable which determines the composition of the
     *        hydrocarbon phases.
     *
     * \note For one-phase models this is disabled.
     */
    static const int compositionSwitchIdx = -10000;

    //! Index of the primary variable for the first solvent
    static const int solventSaturationIdx =
        enableSolvent ? PVOffset + numPhases : -1000;

    //! Index of the primary variable for the first extbo component
    static const int zFractionIdx =
        enableExtbo ? PVOffset + numPhases + numSolvents : -1000;

    //! Index of the primary variable for the first polymer
    static const int polymerConcentrationIdx =
        enablePolymer ? PVOffset + numPhases + numSolvents : -1000;

    //! Index of the primary variable for the second polymer primary variable (molecular weight)
    static const int polymerMoleWeightIdx =
        numPolymers > 1 ? polymerConcentrationIdx + 1 : -1000;

    //! Index of the primary variable for the first MICP component
    static const int microbialConcentrationIdx =
        enableMICP ? PVOffset + numPhases + numSolvents : -1000;

    //! Index of the primary variable for the second MICP component
    static const int oxygenConcentrationIdx =
        numMICPs > 1 ? microbialConcentrationIdx + 1 : -1000;

    //! Index of the primary variable for the third MICP component
    static const int ureaConcentrationIdx =
        numMICPs > 2 ? oxygenConcentrationIdx + 1 : -1000;

    //! Index of the primary variable for the fourth MICP component
    static const int biofilmConcentrationIdx =
        numMICPs > 3 ? ureaConcentrationIdx + 1 : -1000;

    //! Index of the primary variable for the fifth MICP component
    static const int calciteConcentrationIdx =
        numMICPs > 4 ? biofilmConcentrationIdx + 1 : -1000;

    //! Index of the primary variable for the foam
    static const int foamConcentrationIdx =
        enableFoam ? PVOffset + numPhases + numSolvents + numPolymers + numMICPs : -1000;

    //! Index of the primary variable for the salt
    static const int saltConcentrationIdx =
        enableBrine ? PVOffset + numPhases + numSolvents + numExtbos + numPolymers + numMICPs + numFoam : -1000;

    //! Index of the primary variable for temperature
    static const int temperatureIdx  =
        enableEnergy ? PVOffset + numPhases + numSolvents + numExtbos + numPolymers + numMICPs + numFoam + numBrine: - 1000;

    //////////////////////
    // Equation indices
    //////////////////////

    //! \brief returns the index of "active" component
    static unsigned canonicalToActiveComponentIndex(unsigned /*compIdx*/)
    {
        return 0;
    }

    static unsigned activeToCanonicalComponentIndex([[maybe_unused]] unsigned compIdx)
    {
        // assumes canonical oil = 0, water = 1, gas = 2;
        assert(compIdx == 0);
        if(gasEnabled) {
            return 2;
        } else if (waterEnabled) {
            return 1;
        } else {
            assert(oilEnabled);
        }
        return 0;
    }

    //! Index of the continuity equation of the first (and only) phase
    static const int conti0EqIdx = PVOffset + 0;

    //! Index of the continuity equation for the first solvent component
    static const int contiSolventEqIdx =
        enableSolvent ? PVOffset + numPhases : -1000;

    //! Index of the continuity equation for the first extbo component
    static const int contiZfracEqIdx =
        enableExtbo ? PVOffset + numPhases + numSolvents : -1000;

    //! Index of the continuity equation for the first polymer component
    static const int contiPolymerEqIdx =
        enablePolymer ? PVOffset + numPhases + numSolvents : -1000;

    //! Index of the continuity equation for the second polymer component (molecular weight)
    static const int contiPolymerMWEqIdx =
        numPolymers > 1 ? contiPolymerEqIdx + 1 : -1000;

    //! Index of the continuity equation for the first MICP component
    static const int contiMicrobialEqIdx =
        enableMICP ? PVOffset + numPhases + numSolvents : -1000;

    //! Index of the continuity equation for the second MICP component
    static const int contiOxygenEqIdx =
        numMICPs > 1 ? contiMicrobialEqIdx + 1 : -1000;

    //! Index of the continuity equation for the third MICP component
    static const int contiUreaEqIdx =
        numMICPs > 2 ? contiOxygenEqIdx + 1 : -1000;

    //! Index of the continuity equation for the fourth MICP component
    static const int contiBiofilmEqIdx =
        numMICPs > 3 ? contiUreaEqIdx + 1 : -1000;

    //! Index of the continuity equation for the fifth MICP component
    static const int contiCalciteEqIdx =
        numMICPs > 4 ? contiBiofilmEqIdx + 1 : -1000;

    //! Index of the continuity equation for the foam component
    static const int contiFoamEqIdx =
        enableFoam ? PVOffset + numPhases + numSolvents + numPolymers + numMICPs : -1000;

    //! Index of the continuity equation for the salt component
    static const int contiBrineEqIdx =
        enableBrine ? PVOffset + numPhases + numSolvents + numExtbos + numPolymers + numMICPs + numFoam : -1000;

    //! Index of the continuity equation for energy
    static const int contiEnergyEqIdx =
        enableEnergy ? PVOffset + numPhases + numSolvents + numExtbos + numPolymers + numMICPs + numFoam + numBrine: -1000;
};

} // namespace Opm

#endif

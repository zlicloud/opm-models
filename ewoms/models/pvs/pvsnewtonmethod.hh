/*
  Copyright (C) 2008-2013 by Andreas Lauser

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
*/
/*!
 * \file
 *
 * \copydoc Ewoms::PvsNewtonMethod
 */
#ifndef EWOMS_PVS_NEWTON_METHOD_HH
#define EWOMS_PVS_NEWTON_METHOD_HH

#include "pvsproperties.hh"

namespace Ewoms {

/*!
 * \ingroup Newton
 * \ingroup PvsModel
 *
 * \brief A newton solver which is specific to the compositional
 *        multi-phase PVS model.
 */
template <class TypeTag>
class PvsNewtonMethod : public GET_PROP_TYPE(TypeTag, DiscNewtonMethod)
{
    typedef typename GET_PROP_TYPE(TypeTag, DiscNewtonMethod) ParentType;
    typedef typename GET_PROP_TYPE(TypeTag, Simulator) Simulator;
    typedef typename GET_PROP_TYPE(TypeTag, SolutionVector) SolutionVector;

public:
    PvsNewtonMethod(Simulator &simulator) : ParentType(simulator)
    {}

    // HACK which is necessary because GCC 4.4 does not support
    // being a friend of typedefs
/*
protected:
    friend class NewtonMethod<TypeTag>;
    friend class ParentType;
*/

    /*!
     * \copydoc NewtonMethod::endIteration_
     */
    void endIteration_(SolutionVector &uCurrentIter,
                       const SolutionVector &uLastIter)
    {
        ParentType::endIteration_(uCurrentIter, uLastIter);
        this->problem().model().switchPrimaryVars_();
    }
};
} // namespace Ewoms

#endif

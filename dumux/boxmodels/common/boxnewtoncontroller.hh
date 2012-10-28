// -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// vi: set et ts=4 sw=4 sts=4:
/*****************************************************************************
 *   Copyright (C) 2012 by Andreas Lauser                                    *
 *                                                                           *
 *   This program is free software: you can redistribute it and/or modify    *
 *   it under the terms of the GNU General Public License as published by    *
 *   the Free Software Foundation, either version 2 of the License, or       *
 *   (at your option) any later version.                                     *
 *                                                                           *
 *   This program is distributed in the hope that it will be useful,         *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 *   GNU General Public License for more details.                            *
 *                                                                           *
 *   You should have received a copy of the GNU General Public License       *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.   *
 *****************************************************************************/
/*!
 * \file
 *
 * \copydoc Dumux::BoxNewtonController
 */
#ifndef DUMUX_BOX_NEWTON_CONTROLLER_HH
#define DUMUX_BOX_NEWTON_CONTROLLER_HH

#include "boxnewtonconvergencewriter.hh"

#include <dumux/nonlinear/newtoncontroller.hh>
#include <dumux/common/propertysystem.hh>

namespace Dumux {

template <class TypeTag>
class BoxNewtonController;

template <class TypeTag>
class BoxNewtonConvergenceWriter;

namespace Properties {
//! create a type tag for the box specific Newton method
NEW_TYPE_TAG(BoxNewtonMethod, INHERITS_FROM(NewtonMethod));

//! The class dealing with the balance equations
NEW_PROP_TAG(Model);

//! The assembler for the Jacobian matrix
NEW_PROP_TAG(JacobianAssembler);

//! The class storing primary variables plus pseudo primary variables
NEW_PROP_TAG(PrimaryVariables);

//! The number of balance equations.
NEW_PROP_TAG(NumEq);

//! Specifies whether the Jacobian matrix should only be reassembled
//! if the current solution deviates too much from the evaluation point
NEW_PROP_TAG(EnablePartialReassemble);

/*!
 * \brief Specifies whether the update should be done using the line search
 *        method instead of the plain Newton method.
 *
 * Whether this property has any effect depends on whether the line
 * search method is implemented for the actual model's Newton
 * controller's update() method. By default line search is not used.
 */
NEW_PROP_TAG(NewtonUseLineSearch);

//! Enable Jacobian recycling?
NEW_PROP_TAG(EnableJacobianRecycling);

//! Enable partial reassembly?
NEW_PROP_TAG(EnablePartialReassemble);

// set default values
SET_TYPE_PROP(BoxNewtonMethod, NewtonController, Dumux::BoxNewtonController<TypeTag>);
SET_TYPE_PROP(BoxNewtonMethod, NewtonConvergenceWriter, Dumux::BoxNewtonConvergenceWriter<TypeTag>);
SET_SCALAR_PROP(BoxNewtonMethod, NewtonRelTolerance, 1e-8);
SET_SCALAR_PROP(BoxNewtonMethod, NewtonAbsTolerance, 1e-5);
SET_INT_PROP(BoxNewtonMethod, NewtonTargetSteps, 10);
SET_INT_PROP(BoxNewtonMethod, NewtonMaxSteps, 18);
SET_BOOL_PROP(NewtonMethod, NewtonUseLineSearch, false);
}

/*!
 * \ingroup BoxModel
 * \ingroup Newton
 *
 * \brief A Newton controller for models using the box scheme.
 *
 * If you want to specialize only some methods but are happy with the
 * defaults of the reference controller, derive your controller from
 * this class and simply overload the required methods.
 *
 * Usually, this controller should be sufficient for models using the
 * box scheme.
 */
template <class TypeTag>
class BoxNewtonController : public NewtonController<TypeTag>
{
    typedef Dumux::NewtonController<TypeTag> ParentType;

    typedef typename GET_PROP_TYPE(TypeTag, Scalar) Scalar;
    typedef typename GET_PROP_TYPE(TypeTag, Problem) Problem;
    typedef typename GET_PROP_TYPE(TypeTag, Model) Model;
    typedef typename GET_PROP_TYPE(TypeTag, NewtonMethod) NewtonMethod;
    typedef typename GET_PROP_TYPE(TypeTag, JacobianAssembler) JacobianAssembler;
    typedef typename GET_PROP_TYPE(TypeTag, GlobalEqVector) GlobalEqVector;
    typedef typename GET_PROP_TYPE(TypeTag, SolutionVector) SolutionVector;
    typedef typename GET_PROP_TYPE(TypeTag, PrimaryVariables) PrimaryVariables;

    enum { numEq = GET_PROP_VALUE(TypeTag, NumEq) };

public:
    BoxNewtonController(Problem &problem)
        : ParentType(problem)
    {
        enablePartialReassemble_ = GET_PARAM(TypeTag, bool, EnablePartialReassemble);
        enableJacobianRecycling_ = GET_PARAM(TypeTag, bool, EnableJacobianRecycling);

        useLineSearch_ = GET_PARAM(TypeTag, bool, NewtonUseLineSearch);
    }

    /*!
     * \brief Register all run-time parameters for the Newton controller.
     */
    static void registerParameters()
    {
        ParentType::registerParameters();

        REGISTER_PARAM(TypeTag, bool, NewtonUseLineSearch, "Use the line-search update method for the Newton method (warning: slow!)");
    }

    /*!
     * \brief Update the relative error of the solution compared to
     *        the previous iteration.
     *
     * The relative error can be seen as a norm of the difference
     * between the current and the next iteration.
     *
     * \param uCurrentIter The current iterative solution
     * \param uLastIter The solution of the last iteration
     * \param deltaU The difference between the current and the next solution
     */
    void newtonUpdateRelError(const SolutionVector &uCurrentIter,
                              const SolutionVector &uLastIter,
                              const GlobalEqVector &deltaU)
    {
        if (!this->enableRelativeCriterion_ && !enablePartialReassemble_)
            return;

        // calculate the relative error as the maximum relative
        // deflection in any degree of freedom.
        this->error_ = 0;
        for (int i = 0; i < int(uLastIter.size()); ++i) {
            PrimaryVariables uNewI = uLastIter[i];
            uNewI -= deltaU[i];

            Scalar vertError = model_().relativeErrorVertex(i,
                                                            uLastIter[i],
                                                            uNewI);
            this->error_ = std::max(this->error_, vertError);

        }

        this->error_ = this->comm_.max(this->error_);

        Scalar maxError = GET_PARAM(TypeTag, Scalar, NewtonMaxRelError);
        if (this->error_ > maxError)
            DUNE_THROW(NumericalProblem,
                       "Newton: Relative error " << this->error_
                       << " is larger than maximum allowed error of " << maxError);
    }

    /*!
     * \brief Update the absolute error of the solution compared to
     *        the previous iteration.
     *
     * \param uCurrentIter The current iterative solution
     * \param uLastIter The solution of the last iteration
     * \param deltaU The difference between the current and the next solution
     */
    void newtonUpdateAbsError(const SolutionVector &uCurrentIter,
                              const SolutionVector &uLastIter,
                              const GlobalEqVector &deltaU)
    {
        if (!this->enableAbsoluteCriterion_)
            return;
        if (useLineSearch_)
            // the absolute error has already been calculated by
            // updateLineSearch()
            return;

        // we actually have to do the heavy lifting...
        newtonUpdateAbsError_(uCurrentIter, uLastIter, deltaU);
    }

    /*!
     * \brief Update the current solution with a delta vector.
     *
     * The error estimates required for the newtonConverged() and
     * newtonProceed() methods should be updated inside this method.
     *
     * Different update strategies, such as line search and chopped
     * updates can be implemented. The default behavior is just to
     * subtract deltaU from uLastIter, i.e.
     * \f[ u^{k+1} = u^k - \Delta u^k \f]
     *
     * \param uCurrentIter The solution vector after the current iteration
     * \param uLastIter The solution vector after the last iteration
     * \param deltaU The delta as calculated from solving the linear
     *               system of equations. This parameter also stores
     *               the updated solution.
     */
    void newtonUpdate(SolutionVector &uCurrentIter,
                      const SolutionVector &uLastIter,
                      const GlobalEqVector &deltaU)
    {
        // make sure not to swallow non-finite values at this point
        if (!std::isfinite(deltaU.two_norm2()))
            DUNE_THROW(NumericalProblem, "Non-finite update!");

        // compute the vertex and element colors for partial reassembly
        if (enablePartialReassemble_) {
            Scalar minReasmTol = 10*this->tolerance_;
            Scalar maxReasmTol = 1e-4;

            // rationale: the newton method has quadratic convergene1
            Scalar reassembleTol = this->error_*this->error_;
            reassembleTol = std::max(minReasmTol, std::min(maxReasmTol, reassembleTol));
            //Scalar reassembleTol = minReasmTol;

            this->model_().jacobianAssembler().updateDiscrepancy(uLastIter, deltaU);
            this->model_().jacobianAssembler().computeColors(reassembleTol);
        }

        if (useLineSearch_)
            lineSearchUpdate_(uCurrentIter, uLastIter, deltaU);
        else {
            for (unsigned i = 0; i < uLastIter.size(); ++i) {
                uCurrentIter[i] = uLastIter[i];
                uCurrentIter[i] -= deltaU[i];
            }
        }
    }

    /*!
     * \brief Called if the Newton method broke down.
     *
     * This method is called _after_ newtonEnd()
     */
    void newtonFail()
    {
        ParentType::newtonFail();

        model_().jacobianAssembler().reassembleAll();
    }

    /*!
     * \brief Called when the Newton method was successful.
     *
     * This method is called _after_ newtonEnd()
     */
    void newtonSucceed()
    {
        ParentType::newtonSucceed();

        if (enableJacobianRecycling_)
            model_().jacobianAssembler().setMatrixReuseable(true);
        else
            model_().jacobianAssembler().reassembleAll();
    }

protected:
    /*!
     * \brief Returns a reference to the problem.
     */
    Model &model_()
    { return this->method_->problem().model(); }

    /*!
     * \brief Returns a reference to the problem.
     */
    const Model &model_() const
    { return this->method_->problem().model(); }

    /*!
     * \brief Update the absolute error for a given solution.
     */
    void newtonUpdateAbsError_(const SolutionVector &uCurrentIter,
                               const SolutionVector &uLastIter,
                               const GlobalEqVector &deltaU)
    {
        GlobalEqVector tmp(uLastIter.size());
        this->absoluteError_ = this->problem().model().globalResidual(tmp, uCurrentIter);
    }

    /*!
     * \brief Update using the line search algorithm.
     */
    void lineSearchUpdate_(SolutionVector &uCurrentIter,
                           const SolutionVector &uLastIter,
                           const GlobalEqVector &deltaU)
    {
       Scalar lambda = 1.0;
       GlobalEqVector tmp(uLastIter.size());

       while (true) {
           for (unsigned i = 0; i < uCurrentIter.size(); ++i) {
               for (int j = 0; j < numEq; ++j) {
                   uCurrentIter[i][j] = uLastIter[i][j] - lambda*deltaU[i][j];
               }
           }

           // calculate the residual of the current solution
           newtonUpdateAbsError_(uCurrentIter, uLastIter, deltaU);
           if (this->absoluteError_ < this->lastAbsoluteError_ || lambda <= 1.0/8) {
               this->endIterMsg() << ", defect " << this->lastAbsoluteError_ << "->"  << this->absoluteError_ << "@lambda=" << lambda;
               return;
           }

           // try with a smaller update
           lambda /= 2.0;
       }
    }

    bool enablePartialReassemble_;
    bool enableJacobianRecycling_;

    bool useLineSearch_;
};
} // namespace Dumux

#endif

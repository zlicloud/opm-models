#ifndef DUMUX_PRECONDITIONERPDELAB_HH
#define DUMUX_PRECONDITIONERPDELAB_HH

#include<dune/pdelab/backend/istlsolverbackend.hh>

namespace Dune {
namespace PDELab {

template<class TypeTag>
class Exchanger
{
    typedef typename GET_PROP_TYPE(TypeTag, PTAG(Model))   Model;
    typedef typename GET_PROP_TYPE(TypeTag, PTAG(GridView))  GridView;
    enum {
        numEq     = GET_PROP_VALUE(TypeTag, PTAG(NumEq)),
        dim       = GridView::dimension
    };
    typedef typename GET_PROP_TYPE(TypeTag, PTAG(Grid))  Grid;
    typedef typename GET_PROP_TYPE(TypeTag, PTAG(Scalar))    Scalar;
    typedef typename GET_PROP(TypeTag, PTAG(SolutionTypes)) SolutionTypes;
    typedef typename SolutionTypes::VertexMapper            VertexMapper;
    typedef typename GET_PROP(TypeTag, PTAG(PDELabTypes)) PDELabTypes;
    typedef typename PDELabTypes::GridOperatorSpace GridOperatorSpace;
    typedef typename GridOperatorSpace::template MatrixContainer<Scalar>::Type Matrix;
    typedef typename Matrix::block_type BlockType;
    typedef typename GridView::template Codim<dim>::Iterator  VertexIterator;
    typedef typename Grid::Traits::GlobalIdSet IDS;
    typedef typename IDS::IdType IdType;
    typedef typename GET_PROP(TypeTag, PTAG(ReferenceElements)) RefElemProp;
    typedef typename RefElemProp::Container                     ReferenceElements;
    typedef typename RefElemProp::ReferenceElement              ReferenceElement;

public:
    Exchanger(const Model& model)
    : gridView_(model.gridView()), vertexMapper_(model.vertexMapper()), borderIndices_(0)
    {
        gid2Index_.clear();
        index2GID_.clear();

        VertexIterator vertexEndIt = gridView_.template end<dim>();
        for (VertexIterator vertexIt = gridView_.template begin<dim>(); vertexIt != vertexEndIt; ++vertexIt)
        {
//        	std::cout << gridView_.comm().rank() << ": node " << vertexMapper_.map(*vertexIt)
//        			<< " at (" << vertexIt->geometry().corner(0) << ") is of type "
//        			<< vertexIt->partitionType() << ", GID = "
//        			<< gridView_.grid().globalIdSet().id(*vertexIt) << std::endl;
            if (vertexIt->partitionType() == BorderEntity)
            {
                int localIdx = vertexMapper_.map(*vertexIt);
                IdType globalIdx = gridView_.grid().globalIdSet().id(*vertexIt);

                std::pair<IdType,int> g2iPair(globalIdx, localIdx);
                gid2Index_.insert(g2iPair);

                std::pair<int,IdType> i2gPair(localIdx, globalIdx);
                index2GID_.insert(i2gPair);
                
                borderIndices_.push_back(localIdx);
            }
        }
    }

    struct MatEntry
    {
        IdType first;
        BlockType second;
        MatEntry (const IdType& f, const BlockType& s) : first(f),second(s) {}
        MatEntry () {}
    };

    // A DataHandle class to exchange matrix entries
    class MatEntryExchange 
    : public CommDataHandleIF<MatEntryExchange,MatEntry> {
        typedef typename Matrix::RowIterator RowIterator;
        typedef typename Matrix::ColIterator ColIterator;
public:
    //! export type of data for message buffer
    typedef MatEntry DataType;

    //! returns true if data for this codim should be communicated
    bool contains (int dim, int codim) const
    {
        return (codim==dim);
    }

    //! returns true if size per entity of given dim and codim is a constant
    bool fixedsize (int dim, int codim) const
    {
        return false;
    }

    /*! how many objects of type DataType have to be sent for a given entity

	      Note: Only the sender side needs to know this size. 
     */
    template<class EntityType>
    size_t size (EntityType& e) const
    {
//    	std::cout << gridView_.comm().rank() << ": begin loop over vertices.\n";
//        VertexIterator vertexEndIt = gridView_.template end<dim>();
//        for (VertexIterator vertexIt = gridView_.template begin<dim>(); vertexIt != vertexEndIt; ++vertexIt)
//        {
//        	std::cout << gridView_.comm().rank() << ": node " << vertexMapper_.map(*vertexIt)
//        			<< " at (" << vertexIt->geometry().corner(0) << ") is of type "
//        			<< vertexIt->partitionType() << ", GID = "
//        			<< gridView_.grid().globalIdSet().id(*vertexIt) << std::endl;
//        }
//    	std::cout << gridView_.comm().rank() << ": end loop over vertices.\n";
//    	std::cout.flush();
//    	std::cout << gridView_.comm().rank() << ": node " << vertexMapper_.map(e)
//    			<< " on level " << e.level() << " at (" << e.geometry().corner(0) << ") is of type "
//    			<< e.partitionType() << ", GID = "
//    			<< gridView_.grid().globalIdSet().id(e) << std::endl;;
        int i = vertexMapper_.map(e);
        int n = 0;
        	for (ColIterator j = A_[i].begin(); j != A_[i].end(); ++j)
        	{
        		// only count those entries corresponding to border entities
        		typename std::map<int,IdType>::const_iterator it = index2GID_.find(j.index());
        		if (it != index2GID_.end())
        			n++;
        	}
//    	std::cout << gridView_.comm().rank() << ": node " << i << " has sending size " << n << std::endl;
        return n;
    }

    //! pack data from user to message buffer
    template<class MessageBuffer, class EntityType>
    void gather (MessageBuffer& buff, const EntityType& e) const
    {
    		int i = vertexMapper_.map(e);
    		for (ColIterator j = A_[i].begin(); j != A_[i].end(); ++j)
    		{
    			// only send those entries corresponding to border entities
    			typename std::map<int,IdType>::const_iterator it=index2GID_.find(j.index());
    			if (it != index2GID_.end()) {
    				buff.write(MatEntry(it->second,*j));
//    		    	std::cout << gridView_.comm().rank() << ": node " << i << " gathers (" << it->second
//    		    			<< ", " << *j << ") for j = " << j.index() << std::endl;
    			}
    		}
    }

    /*! unpack data from message buffer to user

	      n is the number of objects sent by the sender
     */
    template<class MessageBuffer, class EntityType>
    void scatter (MessageBuffer& buff, const EntityType& e, size_t n)
    {
   		int i = vertexMapper_.map(e);
    		for (size_t k = 0; k < n; k++)
    		{
    			MatEntry m;
    			buff.read(m);
    			// only add entries corresponding to border entities
    			typename std::map<IdType,int>::const_iterator it = gid2Index_.find(m.first);
    			if (it != gid2Index_.end())
    			{
//    		    	std::cout << gridView_.comm().rank() << ": node " << i << " adds " << m.second
//    		    			<< " to j = " << it->second << ", GID = " << m.first << std::endl;
    				A_[i][it->second] += m.second;
    			}
    		}
    }

    //! constructor
    MatEntryExchange (const GridView& gridView, const std::map<IdType,int>& g2i,
            const std::map<int,IdType>& i2g,
            const VertexMapper& vm,
            Matrix& A)
            : gridView_(gridView), gid2Index_(g2i), index2GID_(i2g), vertexMapper_(vm), A_(A)
            {}

private:
    const GridView& gridView_;
    const std::map<IdType,int>& gid2Index_;
    const std::map<int,IdType>& index2GID_;
    const VertexMapper& vertexMapper_;
    Matrix& A_;
    };

    void sumEntries (Matrix& A)
    {
      if (gridView_.comm().size() > 1) 
      {
          MatEntryExchange datahandle(gridView_, gid2Index_, index2GID_, vertexMapper_, A);
          gridView_.communicate(datahandle, InteriorBorder_InteriorBorder_Interface, ForwardCommunication);
      }
    }
    
    const std::vector<int>& borderIndices()
    {
       return borderIndices_; 
    }
    
private:
    const GridView& gridView_;
    std::map<IdType,int> gid2Index_;
    std::map<int,IdType> index2GID_;
    const VertexMapper& vertexMapper_;
    std::vector<int> borderIndices_;
};

// wrapped sequential preconditioner
template<class CC, class GFS, class P>
class NonoverlappingWrappedPreconditioner 
  : public Dune::Preconditioner<typename P::domain_type,typename P::range_type> 
{
public:
  //! \brief The domain type of the preconditioner.
  typedef typename P::domain_type domain_type;
  //! \brief The range type of the preconditioner.
  typedef typename P::range_type range_type;

  // define the category
  enum {
    //! \brief The category the preconditioner is part of.
    category=Dune::SolverCategory::nonoverlapping
  };

      //! Constructor.
  NonoverlappingWrappedPreconditioner (const GFS& gfs_, P& prec_, const CC& cc_, 
                                    const std::vector<int>& borderIndices, const ParallelISTLHelper<GFS>& helper_)
    : gfs(gfs_), prec(prec_), cc(cc_), borderIndices_(borderIndices), helper(helper_)
  {}

  /*!
    \brief Prepare the preconditioner.
  
    \copydoc Preconditioner::pre(domain_type&,range_type&)
  */
  virtual void pre (domain_type& x, range_type& b) 
  {
    prec.pre(x,b);
  }

  /*!
    \brief Apply the precondioner.
  
    \copydoc Preconditioner::apply(domain_type&,const range_type&)
  */
  virtual void apply (domain_type& v, const range_type& d)
  {
    range_type dd(d);
    set_constrained_dofs(cc,0.0,dd);
    prec.apply(v,dd);
    
    Dune::PDELab::AddDataHandle<GFS,domain_type> adddh(gfs,v);
    if (gfs.gridview().comm().size()>1)
      gfs.gridview().communicate(adddh,Dune::InteriorBorder_InteriorBorder_Interface,Dune::ForwardCommunication);
    
    for (int k = 0; k < borderIndices_.size(); k++)
        v[borderIndices_[k]] *= 0.5;
  }

  /*!
    \brief Clean up.
  
    \copydoc Preconditioner::post(domain_type&)
  */
  virtual void post (domain_type& x) 
  {
    prec.post(x);
  }

private:
  const GFS& gfs;
  P& prec;
  const CC& cc;
  const std::vector<int>& borderIndices_;
  const ParallelISTLHelper<GFS>& helper;
};

template<class TypeTag>
class ISTLBackend_NoOverlap_BCGS_ILU
{
	typedef typename GET_PROP_TYPE(TypeTag, PTAG(Model)) Model;
	typedef typename GET_PROP(TypeTag, PTAG(PDELabTypes)) PDELabTypes;
	typedef typename PDELabTypes::GridFunctionSpace GridFunctionSpace;
    typedef typename PDELabTypes::ConstraintsTrafo ConstraintsTrafo;
	typedef Dune::PDELab::ParallelISTLHelper<GridFunctionSpace> PHELPER;

public:
	/*! \brief make a linear solver object

	\param[in] gfs a grid function space
	\param[in] maxiter maximum number of iterations to do
	\param[in] verbose print messages if true
	*/
	explicit ISTLBackend_NoOverlap_BCGS_ILU (Model& model, unsigned maxiter_=5000, int verbose_=1)
	: gfs(model.jacobianAssembler().gridFunctionSpace()), phelper(gfs),
	  maxiter(maxiter_), verbose(verbose_), constraintsTrafo_(model.jacobianAssembler().constraintsTrafo()), 
	  exchanger_(model)
	{}

	/*! \brief compute global norm of a vector

	\param[in] v the given vector
	*/
	template<class Vector>
	typename Vector::ElementType norm (const Vector& v) const
	{
		Vector x(v); // make a copy because it has to be made consistent
		typedef Dune::PDELab::NonoverlappingScalarProduct<GridFunctionSpace,Vector> PSP;
		PSP psp(gfs,phelper);
		psp.make_consistent(x);
		return psp.norm(x);
	}

	/*! \brief solve the given linear system

	\param[in] A the given matrix
	\param[out] z the solution vector to be computed
	\param[in] r right hand side
	\param[in] reduction to be achieved
	*/
	template<class Matrix, class SolVector, class RhsVector>
	void apply(Matrix& A, SolVector& z, RhsVector& r, typename SolVector::ElementType reduction)
	{
		typedef Dune::SeqILU0<Matrix,SolVector,RhsVector> SeqPreCond;
		Matrix B(A);
		exchanger_.sumEntries(B);
		SeqPreCond seqPreCond(B, 0.9);

		typedef Dune::PDELab::NonoverlappingOperator<GridFunctionSpace,Matrix,SolVector,RhsVector> POP;
		POP pop(gfs,A,phelper);
		typedef Dune::PDELab::NonoverlappingScalarProduct<GridFunctionSpace,SolVector> PSP;
		PSP psp(gfs,phelper);
		typedef Dune::PDELab::NonoverlappingWrappedPreconditioner<ConstraintsTrafo, GridFunctionSpace, SeqPreCond> ParPreCond;
		ParPreCond parPreCond(gfs, seqPreCond, constraintsTrafo_, exchanger_.borderIndices(), phelper);

		int verb=0;
		if (gfs.gridview().comm().rank()==0) verb=verbose;
		Dune::BiCGSTABSolver<SolVector> solver(pop,psp,parPreCond,reduction,maxiter,verb);
		Dune::InverseOperatorResult stat;
		solver.apply(z,r,stat);
		res.converged  = stat.converged;
		res.iterations = stat.iterations;
		res.elapsed    = stat.elapsed;
		res.reduction  = stat.reduction;
	}

	/*! \brief Return access to result data */
	const Dune::PDELab::LinearSolverResult<double>& result() const
    				  {
		return res;
    				  }

private:
	const GridFunctionSpace& gfs;
	PHELPER phelper;
	Dune::PDELab::LinearSolverResult<double> res;
	unsigned maxiter;
	int verbose;
	const ConstraintsTrafo& constraintsTrafo_;
	Exchanger<TypeTag> exchanger_;
};

template<class TypeTag>
class ISTLBackend_NoOverlap_Loop_Pardiso
{
	typedef typename GET_PROP_TYPE(TypeTag, PTAG(Model)) Model;
	typedef typename GET_PROP(TypeTag, PTAG(PDELabTypes))  PDELabTypes;
	typedef typename PDELabTypes::GridFunctionSpace GridFunctionSpace;
	typedef typename PDELabTypes::ConstraintsTrafo ConstraintsTrafo;
	typedef Dune::PDELab::ParallelISTLHelper<GridFunctionSpace> PHELPER;

public:
	/*! \brief make a linear solver object

	\param[in] gfs a grid function space
	\param[in] maxiter maximum number of iterations to do
	\param[in] verbose print messages if true
	*/
	explicit ISTLBackend_NoOverlap_Loop_Pardiso (Model& model, unsigned maxiter_=5000, int verbose_=1)
	: gfs(model.jacobianAssembler().gridFunctionSpace()), phelper(gfs),
	  maxiter(maxiter_), verbose(verbose_), constraintsTrafo_(model.jacobianAssembler().constraintsTrafo()),
	  exchanger_(model)
	{}

	/*! \brief compute global norm of a vector

	\param[in] v the given vector
	*/
	template<class Vector>
	typename Vector::ElementType norm (const Vector& v) const
	{
		Vector x(v); // make a copy because it has to be made consistent
		typedef Dune::PDELab::NonoverlappingScalarProduct<GridFunctionSpace,Vector> PSP;
		PSP psp(gfs,phelper);
		psp.make_consistent(x);
		return psp.norm(x);
	}

	/*! \brief solve the given linear system

	\param[in] A the given matrix
	\param[out] z the solution vector to be computed
	\param[in] r right hand side
	\param[in] reduction to be achieved
	*/
	template<class Matrix, class SolVector, class RhsVector>
	void apply(Matrix& A, SolVector& z, RhsVector& r, typename SolVector::ElementType reduction)
	{
		typedef Dune::SeqPardiso<Matrix,SolVector,RhsVector> SeqPreCond;
		Matrix B(A);
		exchanger_.sumEntries(B);
		SeqPreCond seqPreCond(B);

		typedef Dune::PDELab::NonoverlappingOperator<GridFunctionSpace,Matrix,SolVector,RhsVector> POP;
		POP pop(gfs,A,phelper);
		typedef Dune::PDELab::NonoverlappingScalarProduct<GridFunctionSpace,SolVector> PSP;
		PSP psp(gfs,phelper);
		typedef Dune::PDELab::NonoverlappingWrappedPreconditioner<ConstraintsTrafo, GridFunctionSpace, SeqPreCond> ParPreCond;
		ParPreCond parPreCond(gfs, seqPreCond, constraintsTrafo_, exchanger_.borderIndices(), phelper);

		//		typedef Dune::PDELab::NonoverlappingRichardson<GridFunctionSpace,SolVector,RhsVector> PRICH;
		//		PRICH prich(gfs,phelper);
		int verb=0;
		if (gfs.gridview().comm().rank()==0) verb=verbose;
		Dune::BiCGSTABSolver<SolVector> solver(pop,psp,parPreCond,reduction,maxiter,verb);
		Dune::InverseOperatorResult stat;
		solver.apply(z,r,stat);
		res.converged  = stat.converged;
		res.iterations = stat.iterations;
		res.elapsed    = stat.elapsed;
		res.reduction  = stat.reduction;
	}

	/*! \brief Return access to result data */
	const Dune::PDELab::LinearSolverResult<double>& result() const
    				  {
		return res;
    				  }

private:
	const GridFunctionSpace& gfs;
	PHELPER phelper;
	Dune::PDELab::LinearSolverResult<double> res;
	unsigned maxiter;
	int verbose;
	const ConstraintsTrafo& constraintsTrafo_;
	Exchanger<TypeTag> exchanger_;
};



} // namespace PDELab
} // namespace Dumux

#endif

//Solve - \Delta u = 1

#include "FemusInit.hpp"
#include "MultiLevelSolution.hpp"
#include "MultiLevelProblem.hpp"
#include "VTKWriter.hpp"
#include "LinearImplicitSystem.hpp"
#include "NumericVector.hpp"

#include "CurrentElem.hpp"
#include "ElemType_template.hpp"



using namespace femus;





double InitialValueDS(const std::vector < double >& x) {
  return 0.;
}


bool SetBoundaryCondition(const std::vector < double >& x, const char name[], double& value, const int face_name, const double time) {

  bool dirichlet = false;
  value = 0;
  
  const double tolerance = 1.e-5;
  
  if (face_name == 1) {
      dirichlet = true;
        value = 0.;
  }
  
  return dirichlet;
 }


template < class real_num, class real_num_mov >
void AssembleProblem(MultiLevelProblem& ml_prob);



int main(int argc, char** args) {

  // init Petsc-MPI communicator
  FemusInit mpinit(argc, args, MPI_COMM_WORLD);

  // ======= Files ========================
  Files files; 
        files.CheckIODirectories();
        files.RedirectCout();

  // ======= Quad Rule ========================
  std::string fe_quad_rule("seventh");

    // ======= Mesh  ==================
   std::vector<std::string> mesh_files;
   
   mesh_files.push_back("Mesh_1_x.med");
   mesh_files.push_back("Mesh_1_y.med");
   mesh_files.push_back("Mesh_1_z.med");
   mesh_files.push_back("Mesh_2_xy.med");
   mesh_files.push_back("Mesh_2_xz.med");
   mesh_files.push_back("Mesh_2_yz.med");
   mesh_files.push_back("Mesh_3_xyz.med");
   


 for (unsigned int m = 0; m < mesh_files.size(); m++)  {
   
  // define multilevel mesh
  MultiLevelMesh ml_mesh;
  double scalingFactor = 1.;
 
  const bool read_groups = true; //with this being false, we don't read any group at all. Therefore, we cannot even read the boundary groups that specify what are the boundary faces, for the boundary conditions
  
  std::string mesh_file_tot = "./input/" + mesh_files[m];
  
  ml_mesh.ReadCoarseMesh(mesh_file_tot.c_str(), fe_quad_rule.c_str(), scalingFactor, read_groups);
//     ml_mesh.GenerateCoarseBoxMesh(2,0,0,0.,1.,0.,0.,0.,0.,EDGE3,fe_quad_rule.c_str());
//     ml_mesh.GenerateCoarseBoxMesh(0,2,0,0.,0.,0.,1.,0.,0.,EDGE3,fe_quad_rule.c_str());
 
  unsigned numberOfUniformLevels = 4;
  unsigned numberOfSelectiveLevels = 0;
  ml_mesh.RefineMesh(numberOfUniformLevels , numberOfUniformLevels + numberOfSelectiveLevels, NULL);
  ml_mesh.EraseCoarseLevels(numberOfUniformLevels + numberOfSelectiveLevels - 1);
  ml_mesh.PrintInfo();

  // ======= Solution  ==================
  MultiLevelSolution mlSol(&ml_mesh);

    // ******* Print mesh *******
//   mlSol.SetWriter(VTK);  //   mlSol.GetWriter()->SetDebugOutput(true);
//   mlSol.GetWriter()->Write(files.GetOutputPath(), "biquadratic");
//   exit(0);
//   ml_mesh.SetWriter(VTK);  ///@todo this doesn't work and should be removed, no application uses it
//   ml_mesh.GetWriter()->Write(DEFAULT_OUTPUTDIR,"biquadratic", meshToBePrinted);  
    // ******* End print mesh *******
  
  // add variables to mlSol
  mlSol.AddSolution("d_s", LAGRANGE, FIRST/*DISCONTINUOUS_POLYNOMIAL, ZERO*/);
  
  // ======= Solution: Initial Conditions ==================
  mlSol.Initialize("All");    // initialize all variables to zero
  mlSol.Initialize("d_s", InitialValueDS);

  // ======= Solution: Boundary Conditions ==================
  mlSol.AttachSetBoundaryConditionFunction(SetBoundaryCondition);
  mlSol.GenerateBdc("d_s");

  
  // ======= Problem ========================
  // define the multilevel problem attach the mlSol object to it
  MultiLevelProblem ml_prob(&mlSol);

// *************************************
 // this problem is defined on an open boundary mesh, and the boundary mesh can change 
 // as a function of the fracture propagation criterion.
 // Therefore, all the structures may need to be re-allocated after that.
  
 // For now, let us start without propagation and set up the dense matrix.
  
 // The workflow is:
  
 // Read the mesh
  
 // Fill the dense matrix, and solve it (collocation type BEM)
  
 // if propagation occurs, re-dimensionalize all the arrays
 // let us start without propagation first
 // and let us start with STEADY-STATE DDM
// *************************************
  

  ml_prob.SetFilesHandler(&files);
  ml_prob.SetQuadratureRuleAllGeomElems(fe_quad_rule);
  ml_prob.set_all_abstract_fe();
  
//   std::vector < std::vector < const elem_type_templ_base<double, double> *  > > elem_all = ml_prob.evaluate_all_fe<double, double>();
  
    // ======= System ========================
 // add system  in ml_prob as a Linear Implicit System
  LinearImplicitSystem& system = ml_prob.add_system < LinearImplicitSystem > ("Frac");
 
  system.AddSolutionToSystemPDE("d_s");
 
  // attach the assembling function to system
  system.SetAssembleFunction(AssembleProblem<double, double>);

//   system.SetMaxNumberOfLinearIterations(2);
  // initialize and solve the system
  system.SetMgType(F_CYCLE/*F_CYCLE*//*M_CYCLE*/); //it doesn't matter if I use only 1 level
  system.SetOuterSolver(GMRES);
  
  system.init();
  system.MGsolve();
  
    // ======= Print ========================
  // print solutions
  std::vector < std::string > variablesToBePrinted;
  variablesToBePrinted.push_back("all");
  mlSol.SetWriter(VTK);
  mlSol.GetWriter()->SetDebugOutput(true);
  mlSol.GetWriter()->Write(mesh_files[m], files.GetOutputPath(), "biquadratic", variablesToBePrinted);
  
  }
 
 
  return 0;
}


// template < class real_num, class real_num_mov >
// AssembleProblem_interface(MultiLevelProblem& ml_prob)
// 
template < class real_num, class real_num_mov >
void AssembleProblem(MultiLevelProblem& ml_prob) {

  LinearImplicitSystem* mlPdeSys  = &ml_prob.get_system<LinearImplicitSystem> ("Frac");  
  const unsigned level = mlPdeSys->GetLevelToAssemble();
  const bool assembleMatrix = mlPdeSys->GetAssembleMatrix();

  Mesh*                    msh = ml_prob._ml_msh->GetLevel(level);

  MultiLevelSolution*    mlSol = ml_prob._ml_sol;
  Solution*                sol = ml_prob._ml_sol->GetSolutionLevel(level);

  LinearEquationSolver* pdeSys = mlPdeSys->_LinSolver[level];
  SparseMatrix*             KK = pdeSys->_KK;
  NumericVector*           RES = pdeSys->_RES;

  const unsigned  dim = msh->GetDimension();
  unsigned dim2 = (3 * (dim - 1) + !(dim - 1));
  const unsigned maxSize = static_cast< unsigned >(ceil(pow(3, dim)));

  unsigned    iproc = msh->processor_id();

  //=============== Geometry ========================================
  unsigned xType = BIQUADR_FE; // get the finite element type for "x", it is always 2 (LAGRANGE QUADRATIC)
  
  CurrentElem < double > geom_element(dim, msh);            // must be adept if the domain is moving, otherwise double
    
  constexpr unsigned int space_dim = 3;
//***************************************************  


 //******************** quadrature *******************************  
  double weight; 

 //********************* unknowns *********************** 
 //***************************************************  
  const int n_vars = mlPdeSys->GetSolPdeIndex().size();
  std::cout << "************" << n_vars << "************";
  vector <double> phi_u;
  vector <double> phi_u_x; 
  vector <double> phi_u_xx;

  phi_u.reserve(maxSize);
  phi_u_x.reserve(maxSize * space_dim);
  phi_u_xx.reserve(maxSize * dim2);
  
 
  unsigned solIndex_u;
  solIndex_u = mlSol->GetIndex("d_s"); 
  unsigned solFEType_u = mlSol->GetSolutionType(solIndex_u); 

  unsigned solPdeIndex_u;
  solPdeIndex_u = mlPdeSys->GetSolPdeIndex("d_s");

  vector < double >  sol_u;     sol_u.reserve(maxSize);
  vector< int > l2GMap_u;    l2GMap_u.reserve(maxSize);
 //***************************************************  
 //***************************************************  

  
 //***************************************************  
 //********* WHOLE SET OF VARIABLES ****************** 

  vector< int > l2GMap_AllVars; l2GMap_AllVars.reserve(n_vars*maxSize); // local to global mapping
  vector< double >         Res;            Res.reserve(n_vars*maxSize);  // local redidual vector
  vector < double >        Jac;            Jac.reserve(n_vars*maxSize * n_vars*maxSize);
 //***************************************************  


  if (assembleMatrix)  KK->zero();

  
     std::vector < std::vector < double > >  JacI_qp(space_dim);
     std::vector < std::vector < double > >  Jac_qp(dim);
    for (unsigned d = 0; d < dim; d++) { Jac_qp[d].resize(space_dim); }
    for (unsigned d = 0; d < space_dim; d++) { JacI_qp[d].resize(dim); }
    
    double detJac_qp;
  
  
  //prepare Abstract quantities for all fe fams for all geom elems: all quadrature evaluations are performed beforehand in the main function
  std::vector < std::vector < const elem_type_templ_base<double, double> *  > > elem_all;
  ml_prob.get_all_abstract_fe(elem_all);
  
  

  // element loop: each process loops only on the elements that owns
  for (int iel = msh->_elementOffset[iproc]; iel < msh->_elementOffset[iproc + 1]; iel++) {

    geom_element.set_coords_at_dofs_and_geom_type(iel, xType);
        
    const short unsigned ielGeom = geom_element.geom_type();

 //**************** state **************************** 
    unsigned nDof_u     = msh->GetElementDofNumber(iel, solFEType_u);
    sol_u    .resize(nDof_u);
    l2GMap_u.resize(nDof_u);
   // local storage of global mapping and solution
    for (unsigned i = 0; i < sol_u.size(); i++) {
     unsigned solDof_u = msh->GetSolutionDof(i, iel, solFEType_u);
      sol_u[i] = (*sol->_Sol[solIndex_u])(solDof_u);
      l2GMap_u[i] = pdeSys->GetSystemDof(solIndex_u, solPdeIndex_u, i, iel);
    }
 //***************************************************  
 
 //******************** ALL VARS ********************* 
    unsigned nDof_AllVars = nDof_u; 
    int nDof_max    =  nDof_u;   // TODO COMPUTE MAXIMUM maximum number of element dofs for one scalar variable
    
    Res.resize(nDof_AllVars);                  std::fill(Res.begin(), Res.end(), 0.);
    Jac.resize(nDof_AllVars * nDof_AllVars);   std::fill(Jac.begin(), Jac.end(), 0.);
    l2GMap_AllVars.resize(0);                  l2GMap_AllVars.insert(l2GMap_AllVars.end(),l2GMap_u.begin(),l2GMap_u.end());
 //*************************************************** 
    
 //========= gauss value quantities ==================   
	double sol_u_gss = 0.;
	std::vector<double> sol_u_x_gss(space_dim);     std::fill(sol_u_x_gss.begin(), sol_u_x_gss.end(), 0.);
 //===================================================   

      // *** Gauss point loop ***
      for (unsigned ig = 0; ig < ml_prob.GetQuadratureRule(ielGeom).GetGaussPointsNumber(); ig++) {
          
        // *** get gauss point weight, test function and test function partial derivatives ***
	elem_all[ielGeom][xType]->Jacobian_geometry(geom_element.get_coords_at_dofs_3d(), ig, Jac_qp, JacI_qp, detJac_qp, dim, space_dim);
    elem_all[ielGeom][solFEType_u]->shape_funcs_current_elem(ig, JacI_qp, phi_u, phi_u_x, phi_u_xx, dim, space_dim);
    weight = detJac_qp * ml_prob.GetQuadratureRule(ielGeom).GetGaussWeightsPointer()[ig];

// elem_all[ielGeom][solFEType_u]->Jacobian_non_isoparametric_templ( elem_all[ielGeom][xType], geom_element.get_coords_at_dofs_3d(), ig, weight, phi_u, phi_u_x, phi_u_xx, dim, space_dim);

// 	msh->_finiteElement[ielGeom][solFEType_u]->Jacobian(geom_element.get_coords_at_dofs_3d(),    ig, weight,    phi_u,    phi_u_x,    phi_u_xx);


//--------------    
	std::fill(sol_u_x_gss.begin(), sol_u_x_gss.end(), 0.);
	
	for (unsigned i = 0; i < nDof_u; i++) {
	                                                sol_u_gss      += sol_u[i] * phi_u[i];
                   for (unsigned d = 0; d < sol_u_x_gss.size(); d++)   sol_u_x_gss[d] += sol_u[i] * phi_u_x[i * space_dim + d];
          }
//--------------    
          
//==========FILLING WITH THE EQUATIONS ===========
	// *** phi_i loop ***
        for (unsigned i = 0; i < nDof_max; i++) {
	  
//--------------    
	      double laplace_res_du_u_i = 0.;
              for (unsigned kdim = 0; kdim < space_dim; kdim++) {
              if ( i < nDof_u )         laplace_res_du_u_i             +=  (phi_u_x   [i * space_dim + kdim] * sol_u_x_gss[kdim]);
	      }
   
//--------------    
	      
//======================Residuals=======================
          // FIRST ROW
          if (i < nDof_u)                      Res[0      + i] += - weight * ( phi_u[i] * (  -1. ) - laplace_res_du_u_i);
//======================Residuals=======================
	      
          if (assembleMatrix) {
	    
            // *** phi_j loop ***
            for (unsigned j = 0; j < nDof_max; j++) {

//--------------    
              double laplace_mat_du_u = 0.;

              for (unsigned kdim = 0; kdim < space_dim; kdim++) {
              if ( i < nDof_u && j < nDof_u )           laplace_mat_du_u           += (phi_u_x   [i * space_dim + kdim] *
                                                                                       phi_u_x   [j * space_dim + kdim]);
	      }
	      
//--------------    

              //============ delta_state row ============================
              //DIAG BLOCK delta_state - state
		  if ( i < nDof_u && j < nDof_u )       Jac[ (0 + i) * nDof_AllVars   + 	(0 + j) ]  += weight * laplace_mat_du_u;
            } // end phi_j loop
          } // endif assemble_matrix

        } // end phi_i loop
        
      } // end gauss point loop


    RES->add_vector_blocked(Res, l2GMap_AllVars);

    if (assembleMatrix) {
      KK->add_matrix_blocked(Jac, l2GMap_AllVars, l2GMap_AllVars);
    }
    
  } //end element loop for each process

  RES->close();

  if (assembleMatrix) KK->close();
//   KK->print();
//   RES->print();

  // ***************** END ASSEMBLY *******************

  return;
}


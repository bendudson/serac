// Copyright (c) 2019-2020, Lawrence Livermore National Security, LLC and
// other Serac Project Developers. See the top-level LICENSE file for
// details.
//
// SPDX-License-Identifier: (BSD-3-Clause)

#include "serac/physics/utilities/finite_element_state.hpp"

namespace serac {

FiniteElementState::FiniteElementState(mfem::ParMesh& mesh, FiniteElementState::Options&& options)
    : mesh_(&mesh),
      coll_(options.coll ? std::move(options.coll)
                         : std::make_unique<mfem::H1_FECollection>(options.order, mesh.Dimension())),
      space_(std::make_unique<mfem::ParFiniteElementSpace>(
          &mesh, &retrieve(coll_), options.space_dim ? *options.space_dim : mesh.Dimension(), options.ordering)),
      gf_(options.allocate_gf
              ? std::make_unique<mfem::ParGridFunction>(&retrieve(space_))
              : std::make_unique<mfem::ParGridFunction>(&retrieve(space_), static_cast<double*>(nullptr))),
      true_vec_(&retrieve(space_)),
      name_(options.name)
{
  if (options.allocate_gf) {
    retrieve(gf_) = 0.0;
  }
  true_vec_ = 0.0;
}

FiniteElementState::FiniteElementState(mfem::ParMesh& mesh, mfem::ParGridFunction& gf, const std::string& name)
    : mesh_(&mesh), space_(gf.ParFESpace()), gf_(&gf), true_vec_(&retrieve(space_)), name_(name)
{
  coll_         = retrieve(space_).FEColl();
  retrieve(gf_) = 0.0;
  true_vec_     = 0.0;
}

std::optional<axom::sidre::MFEMSidreDataCollection> StateManager::datacoll_;
bool                                                StateManager::is_restart_;

void StateManager::initialize(axom::sidre::DataStore& ds, const std::optional<int> cycle_to_load)
{
  SLIC_ERROR_IF(datacoll_, "Serac's datacollection can only be initialized once");

  const std::string coll_name    = "serac_datacoll";
  auto              global_grp   = ds.getRoot()->createGroup(coll_name + "_global");
  auto              bp_index_grp = global_grp->createGroup("blueprint_index/" + coll_name);
  auto              domain_grp   = ds.getRoot()->createGroup(coll_name);

  // Needs to be configured to own the mesh data so all mesh data is saved to datastore/output file
  const bool owns_mesh_data = true;
  datacoll_.emplace("serac_datacoll", bp_index_grp, domain_grp, owns_mesh_data);
  datacoll_->SetComm(MPI_COMM_WORLD);
  if (cycle_to_load) {
    is_restart_ = true;
    datacoll_->Load(*cycle_to_load);
    datacoll_->SetGroupPointers(ds.getRoot()->getGroup(coll_name + "_global/blueprint_index/" + coll_name),
                                ds.getRoot()->getGroup(coll_name));
    SLIC_ERROR_IF(datacoll_->GetBPGroup()->getNumGroups() == 0,
                  "Loaded datastore is empty, was the datastore created on a "
                  "different number of nodes?");

    datacoll_->UpdateStateFromDS();
    datacoll_->UpdateMeshAndFieldsFromDS();
  } else {
    datacoll_->SetCycle(0);   // Iteration counter
    datacoll_->SetTime(0.0);  // Simulation time
  }
}

FiniteElementState StateManager::newState(mfem::ParMesh& mesh, FiniteElementState::Options&& options)
{
  SLIC_ERROR_IF(!datacoll_, "Serac's datacollection was not initialized - call StateManager::initialize first");
  std::optional<FiniteElementState> state;
  auto                              dc_mesh = datacoll_->GetMesh();
  if (is_restart_) {
    auto field = datacoll_->GetParField(options.name);
    SLIC_INFO("Restoring field from saved data: " << options.name);
    state.emplace(*static_cast<mfem::ParMesh*>(dc_mesh), *field, options.name);
  } else {
    SLIC_INFO("Creating new state for field: " << options.name);
    options.allocate_gf = false;  // We need  to have the datacollection allocate the gridfunction
    state.emplace(mesh, std::move(options));
    if (!dc_mesh) {
      datacoll_->SetMesh(&mesh);
    }
    datacoll_->RegisterField(options.name, &(state->gridFunc()));
    // Now that it's been allocated, we can set it to zero
    state->gridFunc() = 0.0;
  }
  // I think this return is not RVO-eligible, but the move is still relatively cheap
  return std::move(*state);
}

void StateManager::step(const double t, const int cycle)
{
  SLIC_ERROR_IF(!datacoll_, "Serac's datacollection was not initialized - call StateManager::initialize first");
  datacoll_->SetTime(t);
  datacoll_->SetCycle(cycle);
  datacoll_->Save();
}

}  // namespace serac

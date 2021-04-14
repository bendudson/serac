// Copyright (c) 2019-2021, Lawrence Livermore National Security, LLC and
// other Serac Project Developers. See the top-level LICENSE file for
// details.
//
// SPDX-License-Identifier: (BSD-3-Clause)

/**
 * @file quadrature_data.hpp
 *
 * @brief The definition of the QuadratureData class
 */

#pragma once

#include "mfem.hpp"

// namespace serac {

/**
 * @brief Lightweight span, replace with std::span when C++20 available
 */
template <typename T>
class Span {
public:
  /**
   * @brief Constructs a new Span
   * @param[in] ptr The pointer to the data
   * @param[in] size The length of the data (number of elements in the array)
   */
  Span(T* ptr, const std::size_t size) : ptr_(ptr), size_(size) {}

  /**
   * @brief Begin iterator (pointer to first element)
   */
  auto begin() { return ptr_; }
  /**
   * @brief Begin iterator (pointer to first element)
   */
  auto end() { return ptr_ + size_; }

private:
  T*          ptr_;
  std::size_t size_;
};

/**
 * @brief Stores instances of user-defined type for each quadrature point in a mesh
 * @tparam T The type of the per-qpt data
 */
template <typename T>
class QuadratureData {
public:
  /**
   * @brief Constructs using a mesh and polynomial order
   * @param[in] mesh The mesh for which quadrature-point data should be stored
   * @param[in] p The polynomial order of the associated finite elements
   */
  QuadratureData(mfem::Mesh& mesh, const int p);

  /**
   * @brief Retrieves the data for a given quadrature point
   * @param[in] element_idx The index of the desired element within the mesh
   * @param[in] q_idx The index of the desired quadrature point within the element
   */
  T& operator()(const int element_idx, const int q_idx);

  /**
   * @brief Assigns an item to each quadrature point
   * @param[in] item The item to assign
   */
  QuadratureData& operator=(const T& item);

  /**
   * @brief Returns a view over the data
   */
  Span<T> data();

private:
  // FIXME: These will probably need to be MaybeOwningPointers
  // See https://github.com/LLNL/axom/pull/433
  /**
   * @brief Storage layout of @p qfunc_ containing mesh and polynomial order info
   */
  mfem::QuadratureSpace qspace_;
  /**
   * @brief Per-quadrature point data, stored as array of doubles for compatibility with Sidre
   */
  mfem::QuadratureFunction qfunc_;
};

/**
 * @brief "Dummy" specialization, intended to be used as sentinel
 */
template <>
class QuadratureData<void> {
};

// A dummy global so that lvalue references can be bound to something of type QData<void>
// FIXME: There's probably a cleaner way to do this, it's technically a non-const global
// but it's not really mutable because no operations are defined for it
QuadratureData<void> dummy_qdata;

// Hijacks the "vdim" parameter (number of doubles per qpt) to allocate the correct amount of storage
template <typename T>
QuadratureData<T>::QuadratureData(mfem::Mesh& mesh, const int p)
    : qspace_(&mesh, p + 1), qfunc_(&qspace_, sizeof(T) / sizeof(double))
{
}

template <typename T>
T& QuadratureData<T>::operator()(const int element_idx, const int q_idx)
{
  // A view into the quadrature point data
  mfem::Vector view;
  qfunc_.GetElementValues(element_idx, q_idx, view);
  return *(reinterpret_cast<T*>(view.GetData()));
}

template <typename T>
QuadratureData<T>& QuadratureData<T>::operator=(const T& item)
{
  auto span = data();
  std::fill(span.begin(), span.end(), item);
  return *this;
}

template <typename T>
Span<T> QuadratureData<T>::data()
{
  // Number of doubles divided by number of doubles per T
  // FIXME: should this just be a member??
  const auto size = qfunc_.Size() / (sizeof(T) / sizeof(double));
  T*         ptr  = reinterpret_cast<T*>(qfunc_.GetData());
  return Span{ptr, size};
}

// }  // namespace serac

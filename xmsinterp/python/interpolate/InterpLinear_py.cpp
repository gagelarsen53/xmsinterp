//------------------------------------------------------------------------------
/// \file
/// \brief
/// \copyright (C) Copyright Aquaveo 2018. Distributed under the xmsng
///  Software License, Version 1.0. (See accompanying file
///  LICENSE_1_0.txt or copy at http://www.aquaveo.com/xmsng/LICENSE_1_0.txt)
//------------------------------------------------------------------------------

//----- Included files ---------------------------------------------------------
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <boost/shared_ptr.hpp>
#include <xmscore/misc/DynBitset.h>
#include <xmscore/python/misc/PyUtils.h>
#include <xmscore/python/misc/PublicObserver.h>
#include <xmsinterp/interpolate/InterpLinear.h>
#include <xmsinterp/python/interpolate/interpolate_py.h>

//----- Namespace declaration --------------------------------------------------
namespace py = pybind11;

//----- Python Interface -------------------------------------------------------
PYBIND11_DECLARE_HOLDER_TYPE(T, boost::shared_ptr<T>);

void initInterpLinear(py::module &m) {
  py::class_<xms::InterpLinear, xms::InterpBase,
    boost::shared_ptr<xms::InterpLinear>> iLin(m, "InterpLinear");
    iLin.def(py::init(&xms::InterpLinear::New));
  // ---------------------------------------------------------------------------
  // function: __str__
  // ---------------------------------------------------------------------------
    const char* __str__doc = R"pydoc(
        Write the internals to a string.

        Returns:
            str: The internals as a string
    )pydoc";

    iLin.def("__str__", &xms::InterpLinear::ToString,__str__doc);
  // ---------------------------------------------------------------------------
  // function: to_string
  // ---------------------------------------------------------------------------
    const char* to_string_doc = R"pydoc(
        Write the internals to a string.

        Returns:
            str: The internals as a string
    )pydoc";

    iLin.def("to_string", &xms::InterpLinear::ToString, to_string_doc);
  // ---------------------------------------------------------------------------
  // function: set_pts_tris
  // ---------------------------------------------------------------------------
    const char* set_pts_tris_doc = R"pydoc(
        Adds the triangles to the class

        Args:
            vec_pts (iterable): Array of point locations.
            vec_pts (iterable): Array of triangles that references the a_pts 
                array. This array will have a size that is a multiple of 3. The 
                first 3 locations in array represent the first triangle and will
                have indices that correspond to locations in the a_pts array.
    )pydoc";

    iLin.def("set_pts_tris", 
        [](xms::InterpLinear &self, py::iterable pts, py::iterable tris) {
            BSHP<xms::VecPt3d> vec_pts = xms::VecPt3dFromPyIter(pts);
            BSHP<xms::VecInt> vec_tris = xms::VecIntFromPyIter(tris);
            self.SetPtsTris(vec_pts, vec_tris);
        },set_pts_tris_doc, py::arg("vec_pts"),py::arg("vec_tris"));
  // ---------------------------------------------------------------------------
  // function: set_scalars
  // ---------------------------------------------------------------------------
    const char* set_scalars_doc = R"pydoc(
        Set the scalars that will be used to interpolate from.

        Args:
            vec_scalars (iterable): Array of interpolation scalar values.
    )pydoc";

    iLin.def("set_scalars", [](xms::InterpLinear &self, py::iterable scalar) {
            BSHP<xms::VecFlt> vec_scalars = xms::VecFltFromPyIter(scalar);
            return self.SetScalars(vec_scalars);
        },set_scalars_doc, py::arg("vec_scalars"));
  // ---------------------------------------------------------------------------
  // function: interp_to_pt
  // ---------------------------------------------------------------------------
    const char* interp_to_pt_doc = R"pydoc(
        Use the stored triangles to interpolate to a point. Returns 
        extrapolation value if the point is outside the triangles.

        Args:
            point (tuple): Location that is interpolated to.

        Returns:
            float: Interpolated value at a_pt.
    )pydoc";

    iLin.def("interp_to_pt",[](xms::InterpLinear &self, py::tuple pt) -> float {
            xms::Pt3d point = xms::Pt3dFromPyIter(pt);
            return self.InterpToPt(point);
        },interp_to_pt_doc, py::arg("point"));
  // ---------------------------------------------------------------------------
  // function: interp_to_pts
  // ---------------------------------------------------------------------------
    const char* interp_to_pts_doc = R"pydoc(
        Calls interp_to_pt in a loop.

        Args:
            vec_pts (iterable): Locations of points.

        Returns:
            iterable: Interpolated scalar values at a_pts.
    )pydoc";

    iLin.def("interp_to_pts", 
        [](xms::InterpLinear &self, py::iterable pts) -> py::iterable {
            BSHP<xms::VecPt3d> vec_pts = xms::VecPt3dFromPyIter(pts);
            BSHP<xms::VecFlt> vec_scalars(new xms::VecFlt());
            self.InterpToPts(*vec_pts, *vec_scalars);
            return xms::PyIterFromVecFlt(*vec_scalars, 
                                        py::isinstance<py::array>(pts));
        },interp_to_pts_doc, py::arg("vec_pts"));
  // ---------------------------------------------------------------------------
  // function: set_pt_activity
  // ---------------------------------------------------------------------------
    const char* set_pt_activity_doc = R"pydoc(
        Modifies the activity bitset of the class.

        Args:
            bitset (iterable): bitset of the activity of the points.
    )pydoc";

    iLin.def("set_pt_activity", 
        [](xms::InterpLinear &self, py::iterable activity) {
            xms::DynBitset bitset = xms::DynamicBitsetFromPyIter(activity);
            self.SetPtActivity(bitset);
        },set_pt_activity_doc,py::arg("bitset"));
  // ---------------------------------------------------------------------------
  // function: set_tri_activity
  // ---------------------------------------------------------------------------
    const char* set_tri_activity_doc = R"pydoc(
        Modifies the activity bitset of the class.

        Args:
            bitset (iterable): bitset of the activity of the triangles.
    )pydoc";

    iLin.def("set_tri_activity", 
        [](xms::InterpLinear &self, py::iterable activity) {
            xms::DynBitset bitset = xms::DynamicBitsetFromPyIter(activity);
            self.SetTriActivity(bitset);
        },set_tri_activity_doc,py::arg("bitset"));
  // ---------------------------------------------------------------------------
  // function: tri_containing_pt
  // ---------------------------------------------------------------------------
    const char* tri_containing_pt_doc = R"pydoc(
        Find the triangle containing the point

        Args:
            point (tuple): Location used to find a triangle.

        Returns:
            int: Index of triangle containing a_pt. If XM_NONE is returned then 
                no triangle contained the point
    )pydoc";

    iLin.def("tri_containing_pt", 
        [](xms::InterpLinear &self, py::tuple pt) -> int {
            xms::Pt3d point = xms::Pt3dFromPyIter(pt);
            return self.TriContainingPt(point);
        },tri_containing_pt_doc, py::arg("point"));
  // ---------------------------------------------------------------------------
  // function: tri_envelops_containing_pt
  // ---------------------------------------------------------------------------
    const char* tri_envelops_containing_pt_doc = R"pydoc(
        Find all triangle whose envelop contains the point.

        Args:
            point (tuple): Location used to find a triangle.

        Returns:
            array: The indices to triangles whose envelop contains the point
    )pydoc";

    iLin.def("tri_envelops_containing_pt", 
        [](xms::InterpLinear &self, py::tuple pt) -> py::array {
            xms::VecInt tris;
            xms::Pt3d point = xms::Pt3dFromPyIter(pt);
            self.TriEnvelopsContainingPt(point, tris);
            return xms::PyIterFromVecInt(tris);
        },tri_envelops_containing_pt_doc, py::arg("point"));
  // ---------------------------------------------------------------------------
  // function: interp_weights
  // ---------------------------------------------------------------------------
    const char* interp_weights_doc = R"pydoc(
        Use the stored triangles to get interpolation weights for a point. 
        Returns false if the point is outside the triangles.

        Args:
            point (tuple): Location that is interpolated to.

        Returns:
            iterable: Vector of triangle point indices found in this method.
                and Vector of triangle point weights found in this method.
    )pydoc";

    iLin.def("interp_weights", 
        [](xms::InterpLinear &self, py::tuple pt) -> py::iterable {
            xms::VecInt idxs;
            xms::VecDbl wts;
            xms::Pt3d point = xms::Pt3dFromPyIter(pt);
            bool pt_inside = self.InterpWeights(point, idxs, wts);
            py::array ret_idxs = xms::PyIterFromVecInt(idxs, true);
            py::array ret_wts = xms::PyIterFromVecDbl(wts, true);
            return py::make_tuple(pt_inside, ret_idxs, ret_wts);
        },interp_weights_doc, py::arg("point"));
  // ---------------------------------------------------------------------------
  // function: set_extrap_val
  // ---------------------------------------------------------------------------
    const char* set_extrap_val_doc = R"pydoc(
        Set the constant extrapolation value

        Args:
            value (float): The value assigned to extrapolated points
    )pydoc";

    iLin.def("set_extrap_val", &xms::InterpLinear::SetExtrapVal, 
            set_extrap_val_doc, py::arg("value"));
  // ---------------------------------------------------------------------------
  // function: set_trunc
  // ---------------------------------------------------------------------------
    const char* set_trunc_doc = R"pydoc(
        Set the truncation values for the interpolation and turn on truncation.

        Args:
            max (float): The maximum value for truncation.
            min (float): The minimum value for truncation.
    )pydoc";

    iLin.def("set_trunc", &xms::InterpLinear::SetTrunc,set_trunc_doc,
            py::arg("max"), py::arg("min"));
  // ---------------------------------------------------------------------------
  // function: pts
  // ---------------------------------------------------------------------------
    const char* pts_doc = R"pydoc(
        Returns points vector.

        Returns:
            iterable: points vector
    )pydoc";

    iLin.def_property_readonly("pts", []
        (xms::InterpLinear &self) -> py::iterable {
            BSHP<xms::VecPt3d> pts = self.GetPts();
            return xms::PyIterFromVecPt3d(*pts);
        },pts_doc);
  // ---------------------------------------------------------------------------
  // function: tris
  // ---------------------------------------------------------------------------
    const char* tris_doc = R"pydoc(
        Returns triangles vector.

        Returns:
            iterable: triangles vector
    )pydoc";

    iLin.def_property_readonly("tris", []
        (xms::InterpLinear &self) -> py::iterable {
          BSHP<std::vector<int>> tris = self.GetTris();
          return xms::PyIterFromVecInt(*tris, true);
        },tris_doc);
  // ---------------------------------------------------------------------------
  // function: set_use_clough_tocher
  // ---------------------------------------------------------------------------
    const char* set_use_clough_tocher_doc = R"pydoc(
        Set the class to use the Clough Tocher interpolation method. This is a 
        legacy feature from GMS. Frankly it is so similar to linear that there 
        is really no reason to use it but it is here because users complain if 
        features are removed.

        Args:
            on (bool): True/False to indicate if CT should be used.
            observer (Observer): Progress bar to give users feed back on the set
                up process of CT. If you have a really large set of triangles 
                this may take some time.
    )pydoc";

    iLin.def("set_use_clough_tocher", [](xms::InterpLinear &self, bool on,
                              boost::shared_ptr<xms::PublicObserver> observer) {
          self.SetUseCloughTocher(on, observer);
        },set_use_clough_tocher_doc, py::arg("on"), py::arg("observer"));
  // ---------------------------------------------------------------------------
  // function: set_use_nat_neigh
  // ---------------------------------------------------------------------------
    const char* set_use_nat_neigh_doc = R"pydoc(
        Set the class to use natural neighbor (NN) interpolation.

        Args:
            on (bool): True/False to indicate if NN should be used.
            nodal_func (int): Indicates which nodal function to use.
            nd_func_opt (int): Indicates options for the nearest points when 
                computing the nodal functions.
            nd_func_num_nearest_pts (int): The number of nearest points for 
                nodal function computation.
            blend_weights (bool): Option to use a blending function on the 
                calculated weights.
            observer (Observer): Progress bar to give user feedback for 
                generation of the nodal functions.
    )pydoc";

    iLin.def("set_use_nat_neigh", [](xms::InterpLinear &self, bool on, 
                                    int nodal_func, int nd_func_opt,
                                     int nd_func_num_nearest_pts, 
                                     bool blend_weights,
                              boost::shared_ptr<xms::PublicObserver> observer) {
          self.SetUseNatNeigh(on, nodal_func, nd_func_opt, 
                              nd_func_num_nearest_pts, blend_weights, observer);
        },set_use_nat_neigh_doc,
            py::arg("on"), py::arg("nodal_func"), py::arg("nd_func_opt"), 
            py::arg("nd_func_num_nearest_pts"), py::arg("blend_weights"), 
            py::arg("observer")
        );
}

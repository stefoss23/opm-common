#include <opm/parser/eclipse/EclipseState/Schedule/Group/Group2.hpp>
#include <pybind11/stl.h>
#include "export.hpp"


namespace {

  const std::vector<std::string> wellnames( const Group2& g ) {
        return g.wells( );
    }

     int get_vfp_table_nr( const Group2& g ) {
        return g.getGroupNetVFPTable();
    }
}

void python::common::export_Group(py::module& module) {

  py::class_< Group2 >( module, "Group")
    .def_property_readonly( "name", &Group2::name)
    .def_property_readonly( "num_wells", &Group2::numWells)     
    .def( "_vfp_table_nr", &get_vfp_table_nr )
    .def_property_readonly( "well_names", &wellnames );

}

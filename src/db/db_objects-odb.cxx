#include <odb/pre.hxx>

#define ODB_COMMON_QUERY_COLUMNS_DEF
#include "db_objects-odb.hxx"
#undef ODB_COMMON_QUERY_COLUMNS_DEF

namespace odb
{
  // DBConfigKV
  //

  template struct query_columns<
    ::DBConfig,
    id_common,
    query_columns_base< ::DBConfigKV, id_common >::cfg_alias_ >;

  template struct query_columns<
    ::DBConfigKV,
    id_common,
    access::object_traits_impl< ::DBConfigKV, id_common > >;

  template struct pointer_query_columns<
    ::DBConfigKV,
    id_common,
    access::object_traits_impl< ::DBConfigKV, id_common > >;

  const access::object_traits_impl< ::DBConfigKV, id_common >::
  function_table_type*
  access::object_traits_impl< ::DBConfigKV, id_common >::
  function_table[database_count];

  // DBConfig
  //

  template struct query_columns<
    ::DBConfig,
    id_common,
    access::object_traits_impl< ::DBConfig, id_common > >;

  const access::object_traits_impl< ::DBConfig, id_common >::
  function_table_type*
  access::object_traits_impl< ::DBConfig, id_common >::
  function_table[database_count];

  // DBInvocation
  //

  template struct query_columns<
    ::DBInvocation,
    id_common,
    access::object_traits_impl< ::DBInvocation, id_common > >;

  const access::object_traits_impl< ::DBInvocation, id_common >::
  function_table_type*
  access::object_traits_impl< ::DBInvocation, id_common >::
  function_table[database_count];

  // DBResult
  //

  template struct query_columns<
    ::DBInvocation,
    id_common,
    query_columns_base< ::DBResult, id_common >::invocation_alias_ >;

  template struct query_columns<
    ::DBConfig,
    id_common,
    query_columns_base< ::DBResult, id_common >::cfg_alias_ >;

  template struct query_columns<
    ::DBResult,
    id_common,
    access::object_traits_impl< ::DBResult, id_common > >;

  template struct pointer_query_columns<
    ::DBResult,
    id_common,
    access::object_traits_impl< ::DBResult, id_common > >;

  const access::object_traits_impl< ::DBResult, id_common >::
  function_table_type*
  access::object_traits_impl< ::DBResult, id_common >::
  function_table[database_count];

  // DBResourcesInfo
  //

  template struct pointer_query_columns<
    ::DBResult,
    id_common,
    query_columns_base< ::DBResourcesInfo, id_common >::res_alias_ >;

  template struct query_columns<
    ::DBResourcesInfo,
    id_common,
    access::object_traits_impl< ::DBResourcesInfo, id_common > >;

  template struct pointer_query_columns<
    ::DBResourcesInfo,
    id_common,
    access::object_traits_impl< ::DBResourcesInfo, id_common > >;

  const access::object_traits_impl< ::DBResourcesInfo, id_common >::
  function_table_type*
  access::object_traits_impl< ::DBResourcesInfo, id_common >::
  function_table[database_count];

  // DBPapiMeasurement
  //

  template struct pointer_query_columns<
    ::DBResult,
    id_common,
    query_columns_base< ::DBPapiMeasurement, id_common >::res_alias_ >;

  template struct query_columns<
    ::DBPapiMeasurement,
    id_common,
    access::object_traits_impl< ::DBPapiMeasurement, id_common > >;

  template struct pointer_query_columns<
    ::DBPapiMeasurement,
    id_common,
    access::object_traits_impl< ::DBPapiMeasurement, id_common > >;

  const access::object_traits_impl< ::DBPapiMeasurement, id_common >::
  function_table_type*
  access::object_traits_impl< ::DBPapiMeasurement, id_common >::
  function_table[database_count];

  // DBSolution
  //

  template struct pointer_query_columns<
    ::DBResult,
    id_common,
    query_columns_base< ::DBSolution, id_common >::res_alias_ >;

  template struct query_columns<
    ::DBSolution,
    id_common,
    access::object_traits_impl< ::DBSolution, id_common > >;

  template struct pointer_query_columns<
    ::DBSolution,
    id_common,
    access::object_traits_impl< ::DBSolution, id_common > >;

  const access::object_traits_impl< ::DBSolution, id_common >::
  function_table_type*
  access::object_traits_impl< ::DBSolution, id_common >::
  function_table[database_count];

  // DBSolutionJob
  //

  template struct pointer_query_columns<
    ::DBSolution,
    id_common,
    query_columns_base< ::DBSolutionJob, id_common >::sol_alias_ >;

  template struct query_columns<
    ::DBSolutionJob,
    id_common,
    access::object_traits_impl< ::DBSolutionJob, id_common > >;

  template struct pointer_query_columns<
    ::DBSolutionJob,
    id_common,
    access::object_traits_impl< ::DBSolutionJob, id_common > >;

  const access::object_traits_impl< ::DBSolutionJob, id_common >::
  function_table_type*
  access::object_traits_impl< ::DBSolutionJob, id_common >::
  function_table[database_count];

  // DBIntermediate
  //

  template struct pointer_query_columns<
    ::DBResult,
    id_common,
    query_columns_base< ::DBIntermediate, id_common >::res_alias_ >;

  template struct pointer_query_columns<
    ::DBSolution,
    id_common,
    query_columns_base< ::DBIntermediate, id_common >::solution_alias_ >;

  template struct query_columns<
    ::DBIntermediate,
    id_common,
    access::object_traits_impl< ::DBIntermediate, id_common > >;

  template struct pointer_query_columns<
    ::DBIntermediate,
    id_common,
    access::object_traits_impl< ::DBIntermediate, id_common > >;

  const access::object_traits_impl< ::DBIntermediate, id_common >::
  function_table_type*
  access::object_traits_impl< ::DBIntermediate, id_common >::
  function_table[database_count];

  // DBError
  //

  template struct query_columns<
    ::DBError,
    id_common,
    access::object_traits_impl< ::DBError, id_common > >;

  const access::object_traits_impl< ::DBError, id_common >::
  function_table_type*
  access::object_traits_impl< ::DBError, id_common >::
  function_table[database_count];

  // DBExtendedMeasure
  //

  template struct pointer_query_columns<
    ::DBResult,
    id_common,
    query_columns_base< ::DBExtendedMeasure, id_common >::res_alias_ >;

  template struct query_columns<
    ::DBExtendedMeasure,
    id_common,
    access::object_traits_impl< ::DBExtendedMeasure, id_common > >;

  template struct pointer_query_columns<
    ::DBExtendedMeasure,
    id_common,
    access::object_traits_impl< ::DBExtendedMeasure, id_common > >;

  const access::object_traits_impl< ::DBExtendedMeasure, id_common >::
  function_table_type*
  access::object_traits_impl< ::DBExtendedMeasure, id_common >::
  function_table[database_count];

  // ConfigGetterView
  //

  const access::view_traits_impl< ::ConfigGetterView, id_common >::
  function_table_type*
  access::view_traits_impl< ::ConfigGetterView, id_common >::
  function_table[database_count];
}

#include <odb/post.hxx>

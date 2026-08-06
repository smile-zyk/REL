// =============================================================================
//  xdataset -- Value implementation
// =============================================================================

#include "value.h"

#include "data_series.h"

#include <stdexcept>

namespace rel {

// =========================================================================
//  Value
// =========================================================================

Value::Value() : storage_(Measurement()) {}

Value::Value(Measurement m) : storage_(std::move(m)) {}

Value::Value(const DataArray& da)
    : storage_(std::make_shared<DataArray>(da)) {}

Value::Value(std::shared_ptr<DataArray> da) : storage_(std::move(da)) {}

// ---- type queries ----------------------------------------------------------

bool Value::is_measurement() const {
    return storage_.which() == 0;
}

bool Value::is_data_array() const {
    return storage_.which() == 1;
}

// ---- accessors -------------------------------------------------------------

Measurement& Value::as_measurement() {
    return boost::get<Measurement>(storage_);
}

const Measurement& Value::as_measurement() const {
    return boost::get<Measurement>(storage_);
}

DataArray& Value::as_data_array() {
    return *boost::get<std::shared_ptr<DataArray>>(storage_);
}

const DataArray& Value::as_data_array() const {
    return *boost::get<std::shared_ptr<DataArray>>(storage_);
}

// ---- unified metadata ------------------------------------------------------

DataKind Value::data_kind() const {
    if (is_measurement()) return as_measurement().data_kind();
    return as_data_array().data().data_kind();
}

DataType Value::data_type() const {
    if (is_measurement()) return as_measurement().data_type();
    return as_data_array().data().data_type();
}

DataShape Value::data_shape() const {
    if (is_measurement()) return as_measurement().shape();
    return as_data_array().data().data_shape();
}

const Unit& Value::unit() const {
    if (is_measurement()) return as_measurement().unit();
    return as_data_array().data().unit();
}

Index Value::rows() const {
    if (is_measurement()) return 1;
    return static_cast<Index>(as_data_array().data().size());
}

Index Value::element_count() const {
    if (is_measurement()) return as_measurement().element_count();
    return as_data_array().element_count();
}

// ---- unified inspection ----------------------------------------------------

std::vector<std::string> Value::indep_names() const {
    if (is_measurement()) return {};
    return as_data_array().indep_names();
}

bool Value::is_dependent() const {
    if (is_measurement()) return false;
    return as_data_array().data_kind() == DataArrayKind::kDependent;
}

MultiDimensionSpec Value::dimension_spec() const {
    if (is_measurement()) {
        MultiDimensionSpec spec;
        spec.add_regular(1);
        return spec;
    }
    return as_data_array().multi_dimension_spec();
}

// ---- data / indep_data access ----------------------------------------------

DataSeries& Value::data() {
    if (is_measurement()) {
        Measurement m = boost::get<Measurement>(storage_);
        DataType dtype = m.data_type();

        std::unique_ptr<DataSeries> ds;
        if (dtype == DataType::kBoolean) {
            ds.reset(new DataSeries(DataType::kInteger, DataShape::Scalar()));
            ds->resize(1);
            ds->scalar_at<int>(0) = m.as_scalar<bool>() ? 1 : 0;
        } else {
            ds.reset(new DataSeries(m.data_type(), m.shape()));
            ds->append(m);
        }
        storage_ = std::make_shared<DataArray>(
            DataArray::CreateIndependent(std::move(*ds)));
    }
    return as_data_array().data();
}

const DataSeries& Value::data() const {
    if (is_measurement())
        throw std::runtime_error("Value::data(): Measurement-backed Value has no DataSeries");
    return as_data_array().data();
}

DataSeries& Value::indep_data(Index index) {
    if (is_measurement())
        throw std::runtime_error("Value::indep_data: Measurement-backed Value has no independent data");
    return as_data_array().indep_data(index);
}

const DataSeries& Value::indep_data(Index index) const {
    if (is_measurement())
        throw std::runtime_error("Value::indep_data: Measurement-backed Value has no independent data");
    return as_data_array().indep_data(index);
}

DataSeries& Value::indep_data(const std::string& name) {
    if (is_measurement())
        throw std::runtime_error("Value::indep_data: Measurement-backed Value has no independent data");
    return as_data_array().indep_data(name);
}

const DataSeries& Value::indep_data(const std::string& name) const {
    if (is_measurement())
        throw std::runtime_error("Value::indep_data: Measurement-backed Value has no independent data");
    return as_data_array().indep_data(name);
}

void Value::replace_self_data(DataSeries new_self) {
    if (is_measurement()) {
        Measurement m = boost::get<Measurement>(storage_);
        DataType dtype = m.data_type();

        std::unique_ptr<DataSeries> ds;
        if (dtype == DataType::kBoolean) {
            ds.reset(new DataSeries(DataType::kInteger, DataShape::Scalar()));
            ds->resize(1);
            ds->scalar_at<int>(0) = m.as_scalar<bool>() ? 1 : 0;
        } else {
            ds.reset(new DataSeries(m.data_type(), m.shape()));
            ds->append(m);
        }
        storage_ = std::make_shared<DataArray>(
            DataArray::CreateIndependent(std::move(*ds)));
    }
    as_data_array().replace_self_data(std::move(new_self));
}

Value Value::with_self_data(DataSeries new_self) const {
    if (is_measurement()) {
        Measurement m = as_measurement();
        DataType dtype = m.data_type();

        std::unique_ptr<DataSeries> ds;
        if (dtype == DataType::kBoolean) {
            // Boolean is always scalar; convert to Integer 0/1.
            ds.reset(new DataSeries(DataType::kInteger, DataShape::Scalar()));
            ds->append(Measurement::Integer(m.as_scalar<bool>() ? 1 : 0));
        } else {
            ds.reset(new DataSeries(m.data_type(), m.shape()));
            ds->append(m);
        }
        return Value(DataArray::CreateIndependent(std::move(*ds))
                         .with_self_data(std::move(new_self)));
    }
    return Value(std::make_shared<DataArray>(
        as_data_array().with_self_data(std::move(new_self))));
}

// ---- canonicalization ------------------------------------------------------

Value Value::canonicalized() const
{
    if (is_measurement()) {
        const Measurement& m = as_measurement();
        if (m.is_canonicalized()) return *this;
        return Value(m.canonicalized());
    }
    if (is_data_array()) {
        const DataArray& da = as_data_array();
        if (da.data().is_canonicalized()) return *this;

        auto canonical_datas = da.datas();
        canonical_datas[DataArray::kSelf] = da.data().canonicalized();

        DataArrayCreateInfo info;
        info.datas                = std::move(canonical_datas);
        info.multi_dimension_spec = da.multi_dimension_spec();
        info.kind                 = da.data_kind();

        return Value(std::make_shared<DataArray>(std::move(info)));
    }
    return *this;
}

bool Value::is_canonicalized() const
{
    if (is_measurement()) return as_measurement().is_canonicalized();
    if (is_data_array()) return as_data_array().data().is_canonicalized();
    return true;
}

// ---- formatting ------------------------------------------------------------

std::string Value::Format(const std::string& name, int max_rows) const
{
    if (is_measurement())
    {
        const xdataset::Measurement& m = as_measurement();
        return m.to_dataframe(name).to_string(max_rows);
    }

    // DataArray: render with custom or default variable name
    const xdataset::DataArray& da = as_data_array();
    const std::string& header = name.empty() ? "data" : name;
    return da.GetOrCreateDataFrame(header).to_string(max_rows);
}

// ---- convenience factories -------------------------------------------------

Value Value::Real(double v, const Unit& u) {
    return Value(Measurement::Real(v, u));
}

Value Value::Integer(int v, const Unit& u) {
    return Value(Measurement::Integer(v, u));
}

Value Value::Boolean(bool b) {
    return Value(Measurement::Boolean(b));
}

Value Value::String(const std::string& s) {
    return Value(Measurement::String(s));
}

Value Value::Complex(std::complex<double> v, const Unit& u) {
    return Value(Measurement::Complex(v, u));
}

Value Value::Vector(const VecXd& v, const Unit& u) {
    return Value(Measurement::Vector(v, u));
}

Value Value::Vector(const VecXi& v, const Unit& u) {
    return Value(Measurement::Vector(v, u));
}

Value Value::Vector(const VecXcd& v, const Unit& u) {
    return Value(Measurement::Vector(v, u));
}

Value Value::Vector(const VecXs& v) {
    return Value(Measurement::Vector(v));
}

Value Value::Matrix(const MatXd& m, const Unit& u) {
    return Value(Measurement::Matrix(m, u));
}

Value Value::Matrix(const MatXi& m, const Unit& u) {
    return Value(Measurement::Matrix(m, u));
}

Value Value::Matrix(const MatXcd& m, const Unit& u) {
    return Value(Measurement::Matrix(m, u));
}

Value Value::Matrix(const MatXs& m) {
    return Value(Measurement::Matrix(m));
}

Value Value::ArrayReal(const std::vector<double>& v, const Unit& u) {
    return Value(DataArray::CreateIndependent(
        DataSeries::CreateScalarFromVector<double>(v, u)));
}

Value Value::ArrayInteger(const std::vector<int>& v, const Unit& u) {
    return Value(DataArray::CreateIndependent(
        DataSeries::CreateScalarFromVector<int>(v, u)));
}

Value Value::ArrayComplex(const std::vector<std::complex<double>>& v,
                          const Unit& u) {
    return Value(DataArray::CreateIndependent(
        DataSeries::CreateScalarFromVector<std::complex<double>>(v, u)));
}

Value Value::ArrayString(const std::vector<std::string>& v) {
    return Value(DataArray::CreateIndependent(
        DataSeries::CreateScalarFromVector(v)));
}

Value Value::ArrayVector(const std::vector<VecXd>& rows, const Unit& u) {
    DataSeries s(DataType::kReal,
                 {rows.empty() ? Index(0) : rows[0].size()});
    s.set_unit(u);
    for (const auto& row : rows) s.append(Measurement::Vector(row));
    return Value(DataArray::CreateIndependent(std::move(s)));
}

Value Value::ArrayVector(const std::vector<VecXi>& rows, const Unit& u) {
    DataSeries s(DataType::kInteger,
                 {rows.empty() ? Index(0) : rows[0].size()});
    s.set_unit(u);
    for (const auto& row : rows) s.append(Measurement::Vector(row));
    return Value(DataArray::CreateIndependent(std::move(s)));
}

Value Value::ArrayVector(const std::vector<VecXcd>& rows, const Unit& u) {
    DataSeries s(DataType::kComplex,
                 {rows.empty() ? Index(0) : rows[0].size()});
    s.set_unit(u);
    for (const auto& row : rows) s.append(Measurement::Vector(row));
    return Value(DataArray::CreateIndependent(std::move(s)));
}

Value Value::ArrayVector(const std::vector<VecXs>& rows) {
    DataSeries s(DataType::kString,
                 {rows.empty() ? Index(0) : rows[0].dimension(0)});
    for (const auto& row : rows) s.append(Measurement::Vector(row));
    return Value(DataArray::CreateIndependent(std::move(s)));
}

Value Value::ArrayMatrix(const std::vector<MatXd>& rows, const Unit& u) {
    DataSeries s(DataType::kReal,
                 {rows.empty() ? Index(0) : rows[0].rows(),
                  rows.empty() ? Index(0) : rows[0].cols()});
    s.set_unit(u);
    for (const auto& row : rows) s.append(Measurement::Matrix(row));
    return Value(DataArray::CreateIndependent(std::move(s)));
}

Value Value::ArrayMatrix(const std::vector<MatXi>& rows, const Unit& u) {
    DataSeries s(DataType::kInteger,
                 {rows.empty() ? Index(0) : rows[0].rows(),
                  rows.empty() ? Index(0) : rows[0].cols()});
    s.set_unit(u);
    for (const auto& row : rows) s.append(Measurement::Matrix(row));
    return Value(DataArray::CreateIndependent(std::move(s)));
}

Value Value::ArrayMatrix(const std::vector<MatXcd>& rows, const Unit& u) {
    DataSeries s(DataType::kComplex,
                 {rows.empty() ? Index(0) : rows[0].rows(),
                  rows.empty() ? Index(0) : rows[0].cols()});
    s.set_unit(u);
    for (const auto& row : rows) s.append(Measurement::Matrix(row));
    return Value(DataArray::CreateIndependent(std::move(s)));
}

Value Value::ArrayMatrix(const std::vector<MatXs>& rows) {
    DataSeries s(DataType::kString,
                 {rows.empty() ? Index(0) : rows[0].dimension(0),
                  rows.empty() ? Index(0) : rows[0].dimension(1)});
    for (const auto& row : rows) s.append(Measurement::Matrix(row));
    return Value(DataArray::CreateIndependent(std::move(s)));
}

}  // namespace rel

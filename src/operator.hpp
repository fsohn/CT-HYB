#pragma once

#include <iostream>
#include <cassert>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/array.hpp>

enum OPERATOR_TYPE {
  CREATION_OP = 0,
  ANNIHILATION_OP = 1,
  INVALID_OP = 2,
};


//an operator.
//operators are described by the time where they are inserted, as well as their site, flavor, and type (creation/annihilation).
class psi 
{
 public:
  psi() {t_=0; type_=INVALID_OP; flavor_=0; site_=0;};
  psi(double t, OPERATOR_TYPE type, int flavor) {t_=t; type_=type; flavor_=flavor; site_=0;};
  double time() const {return t_;}

  int flavor() const {return flavor_;}  
  OPERATOR_TYPE type() const {return type_;} // 0=create, 1=destroy
  void set_time(double t) {t_ = t;}

  void set_flavor(int flavor) {flavor_ = flavor;}
  void set_type(OPERATOR_TYPE type) {type_ = type;}
 private:
  double t_;
  int site_, flavor_;
  OPERATOR_TYPE type_;
};

inline bool operator<(const psi& t1, const psi& t2) {
  return t1.time() < t2.time();
}

inline bool operator<(const psi& t1, const double t2) {
  return t1.time() < t2;
}

inline bool operator<(const double t1, const psi& t2) {
  return t1 < t2.time();
}

inline bool operator<=(const psi& t1, const double t2) {
  return t1.time() <= t2;
}

inline bool operator<=(const double t1, const psi& t2) {
  return t1 <= t2.time();
}

inline bool operator>(const psi& t1, const psi& t2) {
 return  t1.time() > t2.time();
}

inline bool operator==(const psi& op1, const psi& op2) {
  return op1.time()==op2.time() && op1.type() == op2.type() && op1.flavor()==op2.flavor();
}

inline bool operator!=(const psi& op1, const psi& op2) {
  return !(op1==op2);
}

typedef boost::multi_index::multi_index_container<psi> operator_container_t; //one can use range() with multi_index_container.

inline void print_list(const operator_container_t &operators)
{
  std::cout<<"list: "<<std::endl;
  for(operator_container_t::const_iterator it=operators.begin(); it!=operators.end(); ++it) {
    std::cout<<it->time()<<"["<<it->flavor()<<"]"<<" ";
  }
  std::cout<<std::endl;
}

inline void safe_erase(operator_container_t& operators, const psi& op) {
  operator_container_t::iterator it_target = operators.find(op);
  if (it_target==operators.end()) {
    throw std::runtime_error("Error in safe_erase: op is not found.");
  }
  operators.erase(it_target);
}

inline std::pair<operator_container_t::iterator, bool> safe_insert(operator_container_t& operators, const psi& op) {
  std::pair<operator_container_t::iterator, bool> r = operators.insert(op);
  if(!r.second) {
    print_list(operators);
    std::cerr << "Trying to insert an operator at " << op.time() << " " << op.type() << " " << op.flavor() << std::endl;
    throw std::runtime_error("problem, cannot insert a operator");
  }
  return r;
}

//c^¥dagger(flavor0) c(flavor1) c^¥dagger(flavor2) c(flavor3) ... at the equal time
template<int N>
class EqualTimeOperator {
public:
  EqualTimeOperator() : time_(-1.0) {
    std::fill(flavors_.begin(),flavors_.end(),-1);
  };

  EqualTimeOperator(const boost::array<int,2*N>& flavors, double time=-1.0) : flavors_(flavors), time_(time) {};

  EqualTimeOperator(const int* flavors, double time=-1.0) : time_(time) {
    for (int i=0; i<2*N; ++i) {
      flavors_[i] = flavors[i];
    }
  };

  inline int flavor(int idx) const {
    assert(idx>=0 && idx<2*N);
    return flavors_[idx];
  }

  inline double get_time() const {return time_;}

private:
  boost::array<int,2*N> flavors_;
  double time_;
};

typedef EqualTimeOperator<1> CdagC;

template<int N>
inline bool operator<(const EqualTimeOperator<N>& op1, const EqualTimeOperator<N>& op2) {
  for (int idigit=0; idigit<N; ++idigit) {
    if (op1.flavor(idigit)<op2.flavor(idigit)) {
      return true;
    } else if (op1.flavor(idigit)>op2.flavor(idigit)) {
      return false;
    }
  }
  return false;
}

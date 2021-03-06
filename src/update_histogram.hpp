#pragma once

#include <algorithm>

#include <alps/utilities/mpi.hpp>

/// performs mpi_allreduce() for a valarray of type T.
/** @NOTE Vector `out_vals` is resized */
template<typename T, typename Op>
void all_reduce(const alps::mpi::communicator& comm, const std::valarray<T>& in_vals, std::valarray<T>&out_vals, Op op) {
  assert(in_vals.size()>0);
  out_vals.resize(in_vals.size());
  MPI_Allreduce((void*)&in_vals[0], (void*)&out_vals[0], in_vals.size(), alps::mpi::detail::mpi_type<T>(), alps::mpi::is_mpi_op<Op,T>::op(), comm);
}

template<class T>
void rebin(std::valarray<T>& org_array, int nrebin) {
  const int new_len = org_array.size()/nrebin;
  std::valarray<T> new_array(new_len);
  int count=0;
  for (int i=0; i<new_len; ++i) {
    T sum = 0;
    for (int j=0; j<nrebin; ++j) {
      sum += org_array[count];
      ++count;
    }
    new_array[i] = sum;
  }
  org_array.resize(new_len);
  org_array = new_array;
}

template<class T>
void rebin(std::valarray<T>& org_array, double maxval, double maxval_new, int new_len) {
  std::valarray<T> new_array(0.0,new_len);
  const int old_len = org_array.size();

  assert(maxval_new<=maxval);

  for (int i=0; i<old_len; ++i) {
    const double cent = (i+0.5)*(maxval/old_len);
    const int pos = static_cast<int>(std::floor(new_len*cent/maxval_new));
    if (0 <= pos && pos < new_len) {
      new_array[pos] += org_array[i];
    }
    if (pos > new_len) {
      break;
    }
  }
  org_array.resize(new_len);
  org_array = new_array;
}

class scalar_histogram
{
public:
  scalar_histogram() :
    num_bins_(0),
    num_sample_(0),
    max_val_(0.0),
    sumval(0.0,0),
    sumval2(0.0,0),
    counter(0.0,0)
  {};
  scalar_histogram(int num_bins_, double max_val) : num_bins_(num_bins_), num_sample_(0), max_val_(max_val), sumval(0.0,num_bins_), sumval2(0.0,num_bins_), counter(0.0,num_bins_){};

  void init(int num_bins_, double max_val) {
    this->num_bins_ = num_bins_;
    this->num_sample_ = 0;
    this->max_val_ = max_val;
    sumval.resize(num_bins_,0.0);
    sumval2.resize(num_bins_,0.0);
    counter.resize(num_bins_,0.0);
    sumval = 0.0;
    sumval2 = 0.0;
    counter = 0.0;
  }

  bool add_sample(double distance, double value) {
    const int pos = static_cast<int>(std::floor(num_bins_*distance/max_val_));
    if (0 <= pos && pos < num_bins_) {
      ++num_sample_;
      sumval[pos] += value;
      sumval2[pos] += value*value;
      ++counter[pos];
      return true;
    } else {
      return false;
    }
  }

  std::valarray<double> get_mean() const {
    std::valarray<double> mean(sumval);
    for (int i=0; i<num_bins_; ++i) {
      mean[i] /= static_cast<double>(counter[i]);
    }
    return mean;
  }

  const std::valarray<double>& get_counter() const {
    return counter;
  }

  const std::valarray<double>& get_sumval() const {
    return sumval;
  }

  //maxdist is not updated if we do not have enough data.
  boost::tuple<bool,double> update_cutoff(double cutoff_ratio, double maxdist, double mag, const alps::mpi::communicator& alps_comm) const {
    assert(cutoff_ratio>=0.0 && cutoff_ratio<=1.0);
    assert(mag>=1.0);
    const int min_count = 10;//for stabilization
    const int ndiv = 4;

    const int num_data = counter.size();
    std::valarray<double> counter_gathered(0.0, num_data);
    std::valarray<double> sumval_gathered(0.0, num_data);

    alps_comm.barrier();
    assert(sumval.size()==num_data);
    assert(counter_gathered.size()==num_data);
    assert(sumval_gathered.size()==num_data);
    all_reduce(alps_comm, counter, counter_gathered, std::plus<double>());
    all_reduce(alps_comm, sumval, sumval_gathered, std::plus<double>());
    alps_comm.barrier();

    double maxdist_new = maxdist;

    //update maxdist_new several times
    for (int update=0; update<10; ++update) {
      std::valarray<double> counter_tmp = counter_gathered;
      std::valarray<double> sumval_tmp = sumval_gathered;
      rebin(counter_tmp, max_val_, maxdist_new, ndiv);
      rebin(sumval_tmp, max_val_, maxdist_new, ndiv);

      bool flag = false;
      double maxval = -1.0;
      for (int i=0; i<counter_tmp.size(); ++i) {
        flag = flag || (counter_tmp[i]<min_count);
        if (flag) {
          break;
        }
        maxval = std::max(maxval, sumval_tmp[i]/counter_tmp[i]);
      }
      if (flag || maxval<0) {
        return boost::make_tuple(false,maxdist_new);
      }

      double ratio = (sumval_tmp[ndiv-1]/counter_tmp[ndiv-1])/maxval;
      if (ratio > cutoff_ratio) {
        maxdist_new *= mag;
      } else if (ratio < cutoff_ratio) {
        maxdist_new /= mag;
      }
      //maxdist_new = std::min(maxdist_new,maxdist);
    }//int update=0
    return boost::make_tuple(true,maxdist_new);
  }

  void reset() {
    num_sample_ = 0;
    for (int i=0; i<num_bins_; ++i) {
      sumval[i] = 0.0;
      sumval2[i] = 0.0;
      counter[i] = 0;
    }
  }

  int num_bins() const {
    return num_bins_;
  }

  int get_num_sample() const {
    return num_sample_;
  };

private:
  int num_bins_, num_sample_;
  double max_val_;
  std::valarray<double> sumval, sumval2;
  std::valarray<double> counter;
};

class scalar_histogram_flavors
{
public:
  scalar_histogram_flavors(int num_bins_, double max_val, int flavors) :
    flavors(flavors),
    num_bins(num_bins_),
    max_val(max_val),
    histograms(flavors)
  {
    //for (auto& elem : histograms) {
    for (int ielm=0; ielm<histograms.size(); ++ielm) {
      histograms[ielm].init(num_bins_, max_val);
    }
  };

  bool add_sample(double distance, double value, int flavor) {
    assert(flavor>=0 && flavor<flavors);
    return histograms[flavor].add_sample(distance, value);
  }

  std::valarray<double> get_mean() const {
    std::valarray<double> mean_flavors(num_bins*flavors), mean(num_bins);

    for (int iflavor=0; iflavor<flavors; ++iflavor) {
      mean = histograms[iflavor].get_mean();
      for (int ibin=0; ibin<num_bins; ++ibin) {
        assert(ibin+iflavor*num_bins<mean_flavors.size());
        mean_flavors[ibin+iflavor*num_bins] = mean[ibin];
      }
    }
    return mean_flavors;
  }

  std::valarray<double> get_counter() const {
    std::valarray<double> counter_flavors(num_bins*flavors);

    for (int iflavor=0; iflavor<flavors; ++iflavor) {
      const std::valarray<double>& counter = histograms[iflavor].get_counter();
      for (int ibin=0; ibin<num_bins; ++ibin) {
        assert(ibin+iflavor*num_bins<counter_flavors.size());
        counter_flavors[ibin+iflavor*num_bins] = counter[ibin];
      }
    }
    return counter_flavors;
  }

  std::valarray<double> get_sumval() const {
    std::valarray<double> sumval_flavors(num_bins*flavors);

    for (int iflavor=0; iflavor<flavors; ++iflavor) {
      const std::valarray<double>& sumval = histograms[iflavor].get_sumval();
      for (int ibin=0; ibin<num_bins; ++ibin) {
        assert(ibin+iflavor*num_bins<sumval_flavors.size());
        sumval_flavors[ibin+iflavor*num_bins] = sumval[ibin];
      }
    }
    return sumval_flavors;
  }

  double update_cutoff(double cutoff_ratio, double maxdist, double mag, const alps::mpi::communicator& alps_comm) const {
    double maxdist_new = -1.0;
    //for (auto& elem : histograms) {
    for (int ielm=0; ielm<histograms.size(); ++ielm) {
      boost::tuple<bool,double> r = histograms[ielm].update_cutoff(cutoff_ratio, maxdist, mag, alps_comm);
      maxdist_new = std::max(maxdist_new, boost::get<1>(r));
    }
    assert(maxdist_new>0);
    return std::max(std::min(max_val, maxdist_new), max_val/num_bins);
  }

  void reset() {
    //for (auto& elem : histograms) {
    for (int ielm=0; ielm<histograms.size(); ++ielm) {
      histograms[ielm].reset();
    }
  }

  int get_num_bins() const {
    return num_bins;
  }

private:
  const int flavors, num_bins;
  const double max_val;
  std::vector<scalar_histogram> histograms;
};

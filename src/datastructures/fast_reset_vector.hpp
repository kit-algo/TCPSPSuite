#ifndef FAST_RESET_VECTOR_H
#define FAST_RESET_VECTOR_H

template<class T>
class FastResetVector {
public:
  FastResetVector(size_t size, T && init_value_in)
    : init_value(init_value_in), round(1), data(size, std::pair<T, unsigned int>(init_value, 0))
  {}

  void reset() {
    this->round++;
  }

  const T & operator[](size_t index) const
  {
    if (data[index].second != this->round) {
      return this->init_value;
    } else {
      return data[index].first;
    }
  }

  T & operator[](size_t index)
  {
    if (data[index].second != this->round) {
      data[index] = {this->init_value, this->round};
    }

    return data[index].first;
  }

private:
  T init_value;
  unsigned int round;
  std::vector<std::pair<T, unsigned int>> data;
};

#endif

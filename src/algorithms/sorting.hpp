//
// Created by lukas on 12.02.18.
//

#ifndef TCPSPSUITE_SORTING_HPP
#define TCPSPSUITE_SORTING_HPP

namespace algo {

class DefaultIndexGetter {
public:
	static unsigned int get(unsigned int i) { return i ; }
};

template<class T, class IC, class IndexGetter = DefaultIndexGetter>
void apply_permutation(std::vector<T> & container, const IC & indices)
{
	// TODO can we somehow get rid of this this bool vector?
	std::vector<bool> done(container.size(), false);

	// TODO use moves everywhere!
	for (unsigned int i = 0 ; i < container.size() ; ++i) {
		if (done[i]) {
			continue;
		}

		T cpy = container[i];
		unsigned int index = IndexGetter::get(indices[i]);
		container[i] = container[index];

		while (IndexGetter::get(indices[index]) != i) {
			container[index] = container[IndexGetter::get(indices[index])];
			done[index] = true;
			index = IndexGetter::get(indices[index]);
		}

		container[index] = cpy;
		done[index] = true;
	}
}

} // namespace algo

#endif //TCPSPSUITE_SORTING_HPP

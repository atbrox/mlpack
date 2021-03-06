/**
 * @file cf_main.hpp
 * @author Mudit Raj Gupta
 *
 * Main executable to run CF.
 */

#include <mlpack/core.hpp>

#include <mlpack/methods/amf/amf.hpp>
#include <mlpack/methods/regularized_svd/regularized_svd.hpp>
#include "cf.hpp"

using namespace mlpack;
using namespace mlpack::cf;
using namespace mlpack::amf;
using namespace mlpack::svd;
using namespace std;

// Document program.
PROGRAM_INFO("Collaborating Filtering", "This program performs collaborative "
    "filtering (CF) on the given dataset. Given a list of user, item and "
    "preferences (--input_file) the program will output a set of "
    "recommendations for each user."
    "\n\n"
    "Optionally, the set of query users can be specified with the --query_file "
    "option.  In addition, the number of recommendations to generate can be "
    "specified with the --recommendations (-r) parameter, and the number of "
    "similar users (the size of the neighborhood) to be considered when "
    "generating recommendations can be specified with the --neighborhood (-n) "
    "option."
    "\n\n"
    "The input file should contain a 3-column matrix of ratings, where the "
    "first column is the user, the second column is the item, and the third "
    "column is that user's rating of that item.  Both the users and items "
    "should be numeric indices, not names. The indices are assumed to start "
    "from 0."
    "\n\n"
    "The following optimization algorithms can be used with --algorithm (-a) "
    "parameter: "
    "\n"
    "RegSVD -- Regularized SVD using a SGD optimizer ");

// Parameters for program.
PARAM_STRING_REQ("input_file", "Input dataset to perform CF on.", "i");
PARAM_STRING("query_file", "List of users for which recommendations are to "
    "be generated (if unspecified, then recommendations are generated for all "
    "users).", "q", "");

PARAM_STRING("output_file","File to save output recommendations to.", "o",
    "recommendations.csv");

PARAM_STRING("algorithm", "Algorithm used for matrix factorization.", "a",
    "NMF");

PARAM_INT("recommendations", "Number of recommendations to generate for each "
    "query user.", "r", 5);
PARAM_INT("neighborhood", "Size of the neighborhood of similar users to "
    "consider for each query user.", "n", 5);

PARAM_INT("rank", "Rank of decomposed matrices.", "R", 2);

template<typename Factorizer>
void ComputeRecommendations(Factorizer factorizer,
                            arma::mat& dataset,
                            const size_t numRecs,
                            const size_t neighbourhood,
                            const size_t rank,
                            arma::Mat<size_t>& recommendations)
{
  CF<Factorizer> c(dataset, factorizer, neighbourhood, rank);

  // Reading users.
  const string queryFile = CLI::GetParam<string>("query_file");
  if (queryFile != "")
  {
    // User matrix.
    arma::Mat<size_t> userTmp;
    arma::Col<size_t> users;
    data::Load(queryFile, userTmp, true, false /* Don't transpose. */);
    users = userTmp.col(0);

    Log::Info << "Generating recommendations for " << users.n_elem << " users "
        << "in '" << queryFile << "'." << endl;
    c.GetRecommendations(numRecs, recommendations, users);
  }
  else
  {
    Log::Info << "Generating recommendations for all users." << endl;
    c.GetRecommendations(numRecs, recommendations);
  }
}
                            
#define CR(x) ComputeRecommendations(x, dataset, numRecs, neighborhood, rank, recommendations)

int main(int argc, char** argv)
{
  // Parse command line options.
  CLI::ParseCommandLine(argc, argv);

  // Read from the input file.
  const string inputFile = CLI::GetParam<string>("input_file");
  arma::mat dataset;
  data::Load(inputFile, dataset, true);

  // Recommendation matrix.
  arma::Mat<size_t> recommendations;

  // Get parameters.
  const size_t numRecs = (size_t) CLI::GetParam<int>("recommendations");
  const size_t neighborhood = (size_t) CLI::GetParam<int>("neighborhood");
  const size_t rank = (size_t) CLI::GetParam<int>("rank");

  // Perform decomposition to prepare for recommendations.
  Log::Info << "Performing CF matrix decomposition on dataset..." << endl;
  
  const string algo = CLI::GetParam<string>("algorithm");
  
  if(algo == "NMF") 
    CR(NMFALSFactorizer());  
  else if(algo == "SVDBatch") 
    CR(SparseSVDBatchFactorizer());
  else if(algo == "SVDIncompleteIncremental") 
    CR(SparseSVDIncompleteIncrementalFactorizer());
  else if(algo == "SVDCompleteIncremental")
    CR(SparseSVDCompleteIncrementalFactorizer());
  else if(algo == "RegSVD")
    CR(RegularizedSVD<>());

  const string outputFile = CLI::GetParam<string>("output_file");
  data::Save(outputFile, recommendations);
}

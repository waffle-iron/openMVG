#include <openMVG/voctree/tree_builder.hpp>
#include <openMVG/voctree/database.hpp>
#include <openMVG/voctree/vocabulary_tree.hpp>
#include <openMVG/voctree/descriptor_loader.hpp>
#include <openMVG/features/descriptor.hpp>
#include <openMVG/logger.hpp>

#include <Eigen/Core>

#include <boost/program_options.hpp>

#include <iostream>
#include <fstream>
#include <string>
#include <chrono>

static const int DIMENSION = 128;

using namespace std;

//using namespace boost::accumulators;
namespace po = boost::program_options;

typedef openMVG::features::Descriptor<float, DIMENSION> DescriptorFloat;

typedef std::map<size_t, openMVG::voctree::Document> DocumentMap;

/*
 * This program is used to load the sift descriptors from a list of files and create a vocabulary tree
 */
int main(int argc, char** argv)
{
  int verbosity = 2;
  string weightName;
  string treeName;
  string keylist;
  uint32_t K = 10;
  uint32_t restart = 5;
  uint32_t LEVELS = 6;
  bool sanityCheck = false;

  po::options_description desc("Options:");
  desc.add_options()
          ("help,h", "Print this message")
          ("verbose,v", po::value<int>(&verbosity)->default_value(2), "Verbosity level, 3 should be just enough, 0 to mute")
          ("weights,w", po::value<string>(&weightName)->required(), "Output name for the weight file")
          ("tree,t", po::value<string>(&treeName)->required(), "Output name for the tree file")
          ("keylist,l", po::value<string>(&keylist)->required(), "Path to the list file (list.txt or sfm_data.json) generated by OpenMVG")
          (",k", po::value<uint32_t>(&K)->default_value(10), "The branching factor of the tree")
          ("restart,r", po::value<uint32_t>(&restart)->default_value(5), "Number of times that the kmean is launched for each cluster, the best solution is kept")
          (",L", po::value<uint32_t>(&LEVELS)->default_value(6), "Number of levels of the tree")
          ("sanitycheck,s", po::bool_switch(&sanityCheck)->default_value(sanityCheck), "Perform a sanity check at the end of the creation of the vocabulary tree. The sanity check is a query to the database with the same documents/images useed to train the vocabulary tree");


  po::variables_map vm;

  try
  {
    po::store(po::parse_command_line(argc, argv, desc), vm);

    if(vm.count("help") || (argc == 1))
    {
      std::cout << "This program is used to load the sift descriptors from a list of files and create a vocabulary tree\n"
              << "It takes as input either a list.txt file containing the a simple list of images (bundler format and older OpenMVG version format)\n"
              << "or a sfm_data file (JSON) containing the list of images. In both cases it is assumed that the .desc to load are in the same directory as the input file\n"
              << std::endl << std::endl;

      std::cout << desc << std::endl;
      return EXIT_SUCCESS;
    }
    po::notify(vm);
  }
  catch(boost::program_options::required_option& e)
  {
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    std::cout << "Usage:\n\n" << desc << std::endl;
    return EXIT_FAILURE;
  }
  catch(boost::program_options::error& e)
  {
    std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
    std::cout << "Usage:\n\n" << desc << std::endl;
    return EXIT_FAILURE;
  }


  {
    std::cout << "The program has been called with\n"
            << "tree: " << treeName << std::endl
            << "weights: " << weightName << std::endl
            << "K: " << K << std::endl
            << "L: " << LEVELS << std::endl
            << "keylist: " << keylist << std::endl
            << "restart: " << restart << std::endl
            << "sanity check: " << sanityCheck << std::endl
            << "verbosity: " << verbosity << std::endl << std::endl;
  }

  std::vector<DescriptorFloat> descriptors;

  std::vector<size_t> descRead;
  POPART_COUT("Reading descriptors from " << keylist);
  auto detect_start = std::chrono::steady_clock::now();
  size_t numTotDescriptors = openMVG::voctree::readDescFromFiles(keylist, descriptors, descRead);
  auto detect_end = std::chrono::steady_clock::now();
  auto detect_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(detect_end - detect_start);
  if(descriptors.size() == 0)
  {
    POPART_CERR("No descriptors loaded!!");
    return EXIT_FAILURE;
  }

  POPART_COUT("Done! " << descRead.size() << " sets of descriptors read for a total of " << numTotDescriptors << " features");
  POPART_COUT("Reading took " << detect_elapsed.count() << " sec");

  // Create tree
  openMVG::voctree::TreeBuilder<DescriptorFloat> builder(DescriptorFloat(0));
  builder.setVerbose(verbosity);
  builder.kmeans().setRestarts(restart);
  POPART_COUT("Building a tree of L=" << LEVELS << " levels with a branching factor of k=" << K);
  detect_start = std::chrono::steady_clock::now();
  builder.build(descriptors, K, LEVELS);
  detect_end = std::chrono::steady_clock::now();
  detect_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(detect_end - detect_start);
  POPART_COUT("Tree created in " << ((float) detect_elapsed.count()) / 1000 << " sec");
  POPART_COUT(builder.tree().centers().size() << " centers");
  POPART_COUT("Saving vocabulary tree as " << treeName);
  builder.tree().save(treeName);


  DocumentMap documents;
  // temporary vector used to save all the visual word for each image before adding them to documents
  vector<openMVG::voctree::Word> imgVisualWords;
  POPART_COUT("Quantizing the features");
  size_t offset = 0; ///< this is used to align to the features of a given image in 'feature'
  detect_start = std::chrono::steady_clock::now();
  // pass each feature through the vocabulary tree to get the associated visual word
  // for each read images, recover the number of features in it from descRead and loop over the features
  for(size_t i = 0; i < descRead.size(); ++i)
  {
    // for each image:
    // clear the temporary vector used to save all the visual word and allocate the proper size
    imgVisualWords.clear();
    // allocate as many visual words as the number of the features in the image
    imgVisualWords.resize(descRead[i], 0);

	#pragma omp parallel for
    for(size_t j = 0; j < descRead[i]; ++j)
    {
      //	store the visual word associated to the feature in the temporary list
      imgVisualWords[j] = builder.tree().quantize(descriptors[ j + offset ]);
    }
    // add the vector to the documents
    documents[ i ] = imgVisualWords;

    // update the offset
    offset += descRead[ i ];
  }
  detect_end = std::chrono::steady_clock::now();
  detect_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(detect_end - detect_start);
  POPART_COUT("Feature quantization took " << detect_elapsed.count() << " sec");


  POPART_COUT("Creating the database...");
  // Add each object (document) to the database
  openMVG::voctree::Database db(builder.tree().words());
  POPART_COUT("\tfound " << documents.size() << " documents");
  for(const auto &doc : documents)
  {
    db.insert(doc.first, doc.second);
  }
  POPART_COUT("Database created!");

  // Compute and save the word weights
  POPART_COUT("Computing weights...");
  detect_start = std::chrono::steady_clock::now();
  db.computeTfIdfWeights();
  detect_end = std::chrono::steady_clock::now();
  detect_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(detect_end - detect_start);
  POPART_COUT("Computing weights done in " << detect_elapsed.count() << " sec");
  POPART_COUT("Saving weights as " << weightName);
  db.saveWeights(weightName);


  if(sanityCheck)
  {
    // Now query each document (sanity check)
    std::vector<openMVG::voctree::DocMatch> matches;
    size_t wrong = 0; // count the wrong matches
    double recval = 0;
    POPART_COUT("Sanity check: querying the database with the same documents");
    // for each document
    for(const auto &doc : documents)
    {
      detect_start = std::chrono::steady_clock::now();
      // retrieve the best 4 matches
      db.find(doc.second, 4, matches);
      detect_end = std::chrono::steady_clock::now();
      detect_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(detect_end - detect_start);
      POPART_COUT("query document " << doc.first 
			  << " took " << detect_elapsed.count()
			  << " ms and has " << matches.size() 
			  << " matches\tBest " << matches[0].id 
			  << " with score " << matches[0].score << endl);
      // for each found match print the score, ideally the first one should be the document itself
      for(size_t j = 0; j < matches.size(); ++j)
      {
        POPART_COUT("\t match " << matches[j].id << " with score " << matches[j].score);
        if(matches[j].id / 4 == (doc.first) / 4) recval += 1;
      }

      // if the first one is not the document itself notify and increment the counter
      if(doc.first != matches[0].id)
      {
        ++wrong;
        cout << "##### wrong match for document " << doc.first << endl;
      }

    }

    if(wrong)
      POPART_COUT("there are " << wrong << " wrong matches");
    else
      POPART_COUT("Yay! no wrong matches!");
    POPART_COUT("\nrecval: " << recval / (double) (documents.size()));
  }


  return EXIT_SUCCESS;
}

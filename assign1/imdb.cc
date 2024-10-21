#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include "imdb.h"
#include <cstring>
#include <algorithm>
using namespace std;

const char *const imdb::kActorFileName = "actordata";
const char *const imdb::kMovieFileName = "moviedata";
imdb::imdb(const string& directory) {
  const string actorFileName = directory + "/" + kActorFileName;
  const string movieFileName = directory + "/" + kMovieFileName;  
  actorFile = acquireFileMap(actorFileName, actorInfo);
  movieFile = acquireFileMap(movieFileName, movieInfo);
}

bool imdb::good() const {
  return !( (actorInfo.fd == -1) || 
	    (movieInfo.fd == -1) ); 
}

imdb::~imdb() {
  releaseFileMap(actorInfo);
  releaseFileMap(movieInfo);
}

bool imdb::getCredits(const string &player, vector<film> &films) const
{
  int numberOfArtists = *(int *)(actorFile);                   // First 4 bytes store the total number of artists.
  int *firstOffsetArtists = ((int *)actorFile + 1);            // The first offset in the list of artists.
  int *lastOffsetArtists = (int *)actorFile + numberOfArtists; // The last artist's offset.
  const char *actorFileChar = (char *)actorFile;

  // Search for the player in the sorted list of artist offsets using binary search.
  auto it = std::lower_bound(firstOffsetArtists, lastOffsetArtists + 1, player.c_str(),
                             [actorFileChar](const int &offset, const char *target)
                             {
                               return std::strcmp(actorFileChar + offset, target) < 0;
                             });

  // If the artist is found.
  if (it != lastOffsetArtists)
  {
    // Ensure the player is an exact match.
    if (std::strcmp(actorFileChar + *it, player.c_str()) != 0)
    {
      return false; // No exact match found.
    }

    short numberOfMovies = 0;
    int artistNameSize = strlen(actorFileChar + *it);

    // Ensure the name size is even; add padding if necessary.
    artistNameSize += artistNameSize % 2 == 0 ? 2 : 1;

    // Retrieve the number of movies the artist appeared in.
    numberOfMovies = *(short *)(actorFileChar + *it + artistNameSize);

    // Calculate the offset where the movie list starts, ensuring 4-byte alignment.
    int expectedMoviesOffest = artistNameSize + sizeof(numberOfMovies);
    int restOf4 = expectedMoviesOffest % 4;
    int movieOffset = restOf4 == 0 ? expectedMoviesOffest : (expectedMoviesOffest + (4 - restOf4));

    int *firstOffsetMovie = (int *)(actorFileChar + (*it + movieOffset));

    // Loop over the number of movies and retrieve their details.
    for (uint16_t i = 0; i < numberOfMovies; i++)
    {
      film filmToAdd;
      char *movieNameOffset = (char *)movieFile + *(firstOffsetMovie + i);
      filmToAdd.title = movieNameOffset;
      filmToAdd.year = 1900 + *(movieNameOffset + strlen(movieNameOffset) + sizeof(char));
      films.emplace_back(filmToAdd);
    }
    return true;
  }
  return false;
}

bool imdb::getCast(const film &movie, vector<string> &players) const
{
  int numberOfMovies = *(int *)(movieFile);                  // First 4 bytes store the total number of movies.
  int *firstOffsetMovies = ((int *)movieFile + 1);           // The first offset in the list of movies.
  int *lastOffsetMovies = (int *)movieFile + numberOfMovies; // The last movie's offset.
  const char *MovieFileChar = (char *)movieFile;
  const char *actorFileChar = (char *)actorFile;

  // Search for the movie in the sorted list of movie offsets using binary search.
  auto it = std::lower_bound(firstOffsetMovies, lastOffsetMovies + 1, movie,
                             [MovieFileChar](const int &offset, const film &movie)
                             {
                               film searchMovie;
                               searchMovie.title = MovieFileChar + offset;
                               searchMovie.year = 1900 + *(MovieFileChar + offset + strlen(MovieFileChar + offset) + sizeof(char)); // Year is 1 byte after the title.
                               return searchMovie < movie;
                             });

  // If the movie is found.
  if (it != lastOffsetMovies)
  {
    const char *movieOffset = MovieFileChar + *it; // Base address of the movie data.
    int movieNameSize = strlen(movieOffset) + 1;   // Movie title + null terminator ('\0').

    // The year of release is stored right after the movie name.
    const char *yearOffset = movieOffset + movieNameSize;
    int yearSize = sizeof(char); // The year is a single byte.

    // Determine if padding is needed after the year.
    int totalSize = movieNameSize + yearSize;
    int extraPadding = (totalSize % 2 == 0) ? 0 : 1;

    // The number of actors is a 2-byte short, located after the movie name, year, and padding.
    short numberOfActors = *((short *)(yearOffset + extraPadding + yearSize));

    // Calculate the offset where the actor list starts, ensuring 4-byte alignment.
    int expectedActorOffest = movieNameSize + extraPadding + sizeof(char) + sizeof(numberOfActors);
    int restOf4 = expectedActorOffest % 4;
    int actorOffset = restOf4 == 0 ? expectedActorOffest : (expectedActorOffest + (4 - restOf4));

    // Loop over the number of actors and retrieve their names.
    for (short i = 0; i < numberOfActors; i++)
    {
      int currentActorOffset = *((int *)(movieOffset + actorOffset + i * 4));
      players.emplace_back(actorFileChar + currentActorOffset);
    }
    return true;
  }
  return false;
}

const void *imdb::acquireFileMap(const string& fileName, struct fileInfo& info) {
  struct stat stats;
  stat(fileName.c_str(), &stats);
  info.fileSize = stats.st_size;
  info.fd = open(fileName.c_str(), O_RDONLY);
  return info.fileMap = mmap(0, info.fileSize, PROT_READ, MAP_SHARED, info.fd, 0);
}

void imdb::releaseFileMap(struct fileInfo& info) {
  if (info.fileMap != NULL) munmap((char *) info.fileMap, info.fileSize);
  if (info.fd != -1) close(info.fd);
}
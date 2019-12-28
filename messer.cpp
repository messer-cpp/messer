#include<type_traits>
#include<utility>
#include<iterator>
#include<string>
#include<string_view>
#include<optional>
#include<variant>
#include<filesystem>
#include<fstream>
#include<veiler/hastur.hpp>
#include<veiler/lampads.hpp>
#include<veiler/pegasus.hpp>

#include<boost/preprocessor/cat.hpp>
#include<boost/preprocessor/seq/for_each.hpp>
#include<boost/range/adaptor/indexed.hpp>
#include<boost/range/adaptor/reversed.hpp>
#include<boost/coroutine2/all.hpp>

namespace messer{

template<typename Iterator>
class output_range{
  Iterator be, en;
 public:
  output_range() = default;
  output_range(const output_range&)noexcept = default;
  output_range(output_range&&)noexcept = default;
  output_range& operator=(const output_range&)noexcept = default;
  output_range& operator=(output_range&&)noexcept = default;
  template<typename I, typename J>
  output_range(I&& b, J&& e):be(std::forward<I>(b)), en(std::forward<J>(e)){}
  const Iterator& begin()const{return be;}
  const Iterator& end  ()const{return en;}
  friend std::ostream& operator<<(std::ostream& os, const output_range& r){
    for(auto&& x : r)
      os << x;
    return os;
  }
};

namespace detail{

using has_get_raw = VEILER_HASTUR_TAG_CREATE(get_raw);

template<typename T>class pointer_iterator;
template<typename T>class pointer_iterator<T*>{
  T* ptr;
  using self = pointer_iterator<T*>;
 public:
  pointer_iterator() = default;
  pointer_iterator(T* ptr):ptr(ptr){}
  pointer_iterator(const pointer_iterator&) = default;
  using value_type = T;
  using difference_type = std::ptrdiff_t;
  using pointer = value_type*;
  using reference = value_type&;
  using iterator_category = std::random_access_iterator_tag;
  value_type operator*()const{return *ptr;}
  self& operator++(){++ptr;return *this;}
  self operator++(int){self t = *this; ++(*this); return t;}
  self& operator+=(std::size_t n){ptr += n;return *this;}
  self& operator--(){--ptr;return *this;}
  self operator--(int){self t = *this;--(*this);return t;}
  self operator-=(std::size_t n){ptr -= n;return *this;}
  friend self operator+ (const self& lhs, std::size_t n){self t = lhs; t += n; return t;}
  friend self operator- (const self& lhs, std::size_t n){self t = lhs; t -= n; return t;}
  friend std::ptrdiff_t operator- (const self& lhs, const self& rhs){return lhs.ptr-rhs.ptr;}
  friend bool operator==(const self& lhs, const self& rhs)noexcept{return lhs.ptr == rhs.ptr;}
  friend bool operator!=(const self& lhs, const self& rhs)noexcept{return !(lhs == rhs);}
};

template<typename T>
using to_inheritable_iterator = std::conditional_t<std::is_pointer<T>::value, pointer_iterator<T>, T>;

}//namespace detail

template<typename FilterIterator, std::enable_if_t<veiler::hastur<detail::has_get_raw>::func<FilterIterator>{}>* = nullptr>
inline auto&& get_raw(FilterIterator&& it){
  return std::forward<FilterIterator>(it).get_raw();
}

template<typename Iterator, std::enable_if_t<!veiler::hastur<detail::has_get_raw>::func<Iterator>{}>* = nullptr>
inline auto&& get_raw(Iterator&& it){
  return std::forward<Iterator>(it);
}

struct annotation_type{
  std::string_view filename;
  std::size_t line;
  std::size_t column;
};

template<typename T>
class annotation_range{
  class annotation_iterator:public detail::to_inheritable_iterator<decltype(std::begin(std::declval<T>()))>{
    using self = annotation_iterator;
    using parent = detail::to_inheritable_iterator<decltype(std::begin(std::declval<T>()))>;
    annotation_type data;
   public:
    annotation_iterator() = default;
    annotation_iterator(const parent& it, std::string_view fn, std::size_t l = 1, std::size_t c = 1):parent{it}, data{fn, l, c}{}
    annotation_iterator(const parent& it, const annotation_type& an):parent{it}, data{an}{}
    annotation_iterator(const self&) = default;
    std::string_view get_filename()const{return data.filename;}
    std::size_t get_line()const{return data.line;}
    std::size_t get_column()const{return data.column;}
    self& operator++(){
      if(**this == '\n'){
        ++data.line;
        data.column = 1;
      }
      else
        ++data.column;
      ++*static_cast<parent*>(this);
      return *this;
    }
    self operator++(int){self t = *this; ++(*this); return t;}
    const annotation_type& get_annotate()const{return data;}
    const parent& get_inner()const{return *this;}
  };
  annotation_type annot_data;
  T&& t;
 public:
  using iterator = annotation_iterator;
  using const_iterator = iterator;
  using value_type = decltype(*std::declval<decltype(std::begin(std::declval<T>()))>());
  annotation_range(annotation_type anno, T&& data):annot_data(std::move(anno)), t(std::forward<T>(data)){}
  annotation_iterator begin()const{return annotation_iterator{t.begin(), annot_data};}
  annotation_iterator end()const{return annotation_iterator{t.end(), annot_data};}
};

class annotation{
  annotation_type data;
 public:
  annotation(const std::string_view& filename, std::size_t l = 1, std::size_t c = 1):data{filename, l, c}{}
  annotation(const annotation_type& anno):data{anno}{}
  template<typename T>
  friend constexpr auto operator|(T&& t, annotation&& a)noexcept{
    return annotation_range<T>{std::move(a.data), std::forward<T>(t)};
  }
};

template<typename T>
class filter_iterator : T{
  using self = filter_iterator<T>;
  std::optional<typename T::value_type> t;
 public:
  using typename T::value_type;
  template<typename... Args>
  explicit filter_iterator(Args&&... args):T{std::forward<Args>(args)...}{}
  filter_iterator(const filter_iterator& other):T{other}{}
  filter_iterator& operator=(const filter_iterator& other){static_cast<T&>(*this) = other; return *this;}
  value_type& operator*(){t = this->dereference();return *t;}
  value_type operator*()const{return this->dereference();}
  value_type* operator->(){return &**this;}
  self& operator++(){this->next();return *this;}
  self operator++(int){self t = *this; ++(*this); return t;}
  self& operator+=(std::size_t n){while(n-->0)++(*this);return *this;}
  auto&& get_raw(){return this->get_raw_iterator();}
  auto&& get_raw()const{return this->get_raw_iterator();}
  friend self operator+(const self& lhs, std::size_t n){self t = lhs; t += n; return t;}
  friend bool operator==(const self& lhs, const self& rhs)noexcept{return lhs.is_equal(rhs);}
  friend bool operator!=(const self& lhs, const self& rhs)noexcept{return !(lhs == rhs);}
};

}

namespace std{

template<typename T>struct iterator_traits<messer::filter_iterator<T>>{
  using value_type = typename messer::filter_iterator<T>::value_type;
  using iterator = messer::filter_iterator<T>;
  using iterator_category = forward_iterator_tag;
  using reference = value_type&;
  using difference_type = std::ptrdiff_t;
  using pointer = value_type*;
};

}

namespace veiler{

template<typename T>
struct hash<messer::filter_iterator<T>> : hash<typename messer::filter_iterator<T>::value_type>{
  constexpr hash() = default;
  constexpr hash(const hash&) = default;
  constexpr hash(hash&&) = default;
  ~hash() = default;
  hash& operator=(const hash&) = default;
  hash& operator=(hash&&) = default;
  using result_type = std::size_t;
  using argument_type = messer::filter_iterator<T>;
  std::size_t operator()(const argument_type& key)const noexcept{
    return veiler::hash<typename messer::filter_iterator<T>::value_type>::operator()(*key);
  }
};

}

namespace messer{

template<typename T, template<typename>class IteratorImpl>
class filter_range{
  T t;
 public:
  using iterator = filter_iterator<IteratorImpl<std::decay_t<T>>>;
  using const_iterator = iterator;
  using value_type = typename iterator::value_type;
  filter_range(T&& t):t{std::forward<T>(t)}{}
  iterator begin()const{using std::begin;using std::end;return iterator{begin(t), end(t)};}
  iterator end  ()const{                 using std::end;return iterator{end  (t), end(t)};}
  friend std::ostream& operator<<(std::ostream& os, const filter_range<T, IteratorImpl>& rhs){
    for(auto&& x : rhs)
      os << x;
    return os;
  }
};

class phase1_t{
  template<typename T>
  struct crlf_to_lf_iterator_impl{
    using iterator = typename T::const_iterator;
   private:
    iterator b;
    iterator e;
    bool is_target()const noexcept{
      if(*b != '\r')
        return false;
      const auto next = std::next(b);
      return next != e && *next == '\n';
    }
   public:
    using value_type = std::enable_if_t<std::is_convertible<typename T::value_type, char>::value, char>;
    crlf_to_lf_iterator_impl() = default;
    crlf_to_lf_iterator_impl(const iterator& b, const iterator& e):b{b}, e{e}{}
    crlf_to_lf_iterator_impl(const crlf_to_lf_iterator_impl&) = default;
    crlf_to_lf_iterator_impl& operator=(const crlf_to_lf_iterator_impl& other){b = other.b; e = other.e;return *this;}
    value_type dereference()const{
      return *b;
    }
    
    void next(){
      ++b;
      if(is_target())
        ++b;
    }
    bool is_equal(const crlf_to_lf_iterator_impl& other)const noexcept{return b == other.b;}
          auto& get_raw_iterator()     {return get_raw(b);}
    const auto& get_raw_iterator()const{return get_raw(b);}
  };
  template<typename T>
  class trigraph_iterator_impl{
    static char trigraph_convert(char c){
      switch(c){
        case '=' : return '#';
        case '(' : return '[';
        case '/' : return '\\';
        case ')' : return ']';
        case '\'': return '^';
        case '<' : return '{';
        case '!' : return '|';
        case '>' : return '}';
        case '-' : return '~';
      }
      return -1;
    }
   public:
    using iterator = typename T::const_iterator;
   private:
    iterator b;
    iterator e;
    bool is_trigraph()const noexcept{
      if(*b != '?')
        return false;
      auto next = std::next(b);
      if(next == e || *next != '?')
        return false;
      next = std::next(next);
      return next != e && trigraph_convert(*next) != -1;
    }
   public:
    using value_type = std::enable_if_t<std::is_convertible<typename T::value_type, char>::value, char>;
    trigraph_iterator_impl() = default;
    trigraph_iterator_impl(const iterator& b, const iterator& e):b{b}, e{e}{}
    trigraph_iterator_impl(const trigraph_iterator_impl&) = default;
    trigraph_iterator_impl& operator=(const trigraph_iterator_impl& other){b = other.b;return *this;}
    value_type dereference()const{
      if(is_trigraph())
        return trigraph_convert(*std::next(b,2));
      return *b;
    }
    void next(){
      if(is_trigraph())
        std::advance(b, 3);
      else
        ++b;
    }
    bool is_equal(const trigraph_iterator_impl& other)const noexcept{return b == other.b;}
          auto& get_raw_iterator()     {return get_raw(b);}
    const auto& get_raw_iterator()const{return get_raw(b);}
  };
  template<typename T>
  using trigraph = filter_range<T, trigraph_iterator_impl>;
  template<typename T>
  using crlf_to_lf = filter_range<T, crlf_to_lf_iterator_impl>;
 public:
  template<typename T>
  friend constexpr auto operator|(T&& t, const phase1_t&)noexcept{
    return /*trigraph<*/crlf_to_lf<T>/*>*/{{std::forward<T>(t)}};
  }
};

class phase2_t{
  template<typename T>
  struct backslash_eol_iterator_impl{
    using iterator = typename T::const_iterator;
   private:
    iterator b;
    iterator e;
    bool is_target()const noexcept{
      if(*b != '\\')
        return false;
      const auto next = std::next(b);
      return next != e && *next == '\n';
    }
    void skip(){
      while(is_target())
        std::advance(b, 2);
    }
   public:
    using value_type = std::enable_if_t<std::is_convertible<typename T::value_type, char>::value, char>;
    backslash_eol_iterator_impl() = default;
    backslash_eol_iterator_impl(const iterator& b, const iterator& e):b{b}, e{e}{skip();}
    backslash_eol_iterator_impl(const backslash_eol_iterator_impl&) = default;
    backslash_eol_iterator_impl& operator=(const backslash_eol_iterator_impl& other){b = other.b;return *this;}
    value_type dereference()const{
      return *b;
    }
    void next(){
      ++b;
      skip();
    }
    bool is_equal(const backslash_eol_iterator_impl& other)const noexcept{return b == other.b;}
          auto& get_raw_iterator()     {return get_raw(b);}
    const auto& get_raw_iterator()const{return get_raw(b);}
  };
  template<typename T>
  using backslash_eol = filter_range<T, backslash_eol_iterator_impl>;
 public:
  template<typename T>
  friend constexpr auto operator|(T&& t, const phase2_t&)noexcept{
    return backslash_eol<T>{std::forward<T>(t)};
  }
};

#define MESSER_DECL_TOKEN_TYPE(sequence) \
enum class token_type{\
  BOOST_PP_SEQ_ENUM(sequence)\
  , END\
};\
inline std::ostream& operator<<(std::ostream& os, token_type t){\
  switch(t){\
    BOOST_PP_SEQ_FOR_EACH(MESSER_DECL_TOKEN_TYPE_I, _, sequence)\
    default:os<<"unknown("<<static_cast<std::underlying_type_t<token_type>>(t)<<')';\
  }\
  return os;\
}
#define MESSER_DECL_TOKEN_TYPE_I(r, _, name) case token_type::name: os << BOOST_PP_STRINGIZE(name);break;

#define PUNCTUATORS \
  (punctuator)(punctuator_hash)(punctuator_hashhash)(punctuator_left_parenthesis)(punctuator_comma)(punctuator_ellipsis)(punctuator_right_parenthesis)(punctuator_left_shift)(punctuator_right_shift)(punctuator_less_equal)(punctuator_greater_equal)(punctuator_less)(punctuator_greater)(punctuator_equalequal)(punctuator_not_equal)(punctuator_logical_or)(punctuator_logical_and)(punctuator_logical_not)(punctuator_ampersand)(punctuator_asterisk)(punctuator_plus)(punctuator_minus)(punctuator_division)(punctuator_modulo)(punctuator_bitwise_or)(punctuator_bitwise_xor)(punctuator_bitwise_not)(punctuator_question)(punctuator_colon)
#define IDENTIFIERS \
  (identifier)(identifier_include)(identifier_define)(identifier_undef)(identifier_line)(identifier_error)(identifier_pragma)(identifier_if)(identifier_ifdef)(identifier_ifndef)(identifier_elif)(identifier_else)(identifier_endif)(identifier_defined)(identifier_time_)(identifier_date_)(identifier_file_)(identifier_line_)(identifier_pragma_op)(identifier_has_include)(identifier_true)(identifier_false)
MESSER_DECL_TOKEN_TYPE(
  (empty)
  (white_space)
  (eol)
  (header_name)
  PUNCTUATORS
  (character_literal)
  (string_literal)
  (pp_number)
  IDENTIFIERS
  (unclassified_character)
)
#undef MESSER_DECL_TOKEN_TYPE_I
#undef MESSER_DECL_TOKEN_TYPE

inline bool is_white_spaces(const token_type& t)noexcept{switch(t){case token_type::white_space: case token_type::eol: return true;default:return false;}}
#define MESSER_DECL_TOKEN_TYPE(r, _, name) case token_type::name:
inline bool is_punctuator(const token_type& t)noexcept{switch(t){BOOST_PP_SEQ_FOR_EACH(MESSER_DECL_TOKEN_TYPE, _, PUNCTUATORS)return true; default: return false;}}
inline bool is_identifier(const token_type& t)noexcept{switch(t){BOOST_PP_SEQ_FOR_EACH(MESSER_DECL_TOKEN_TYPE, _, IDENTIFIERS)return true; default: return false;}}

template<typename StrT>
class token{
  StrT str;
  token_type tt;
 public:
  template<typename T>
  token(T&& str, token_type tok):str{std::forward<T>(str)}, tt{tok}{}
  token_type type()const{return tt;}
  token_type& type(){return tt;}
  const StrT& get()const{return str;}
  friend std::ostream& operator<<(std::ostream& os, const token& tkn){return os << tkn.str;}
};

#define RULE VEILER_PEGASUS_RULE
#define AUTO_RULE VEILER_PEGASUS_AUTO_RULE
#define INLINE_RULE VEILER_PEGASUS_INLINE_RULE

class phase3_t{
  struct lexer : veiler::pegasus::parsers<lexer>{
    static auto entrypoint(){
      return instance().preprocessing_token;
    }
    static auto inner_include(){
      return instance().rule_inner_include;
    }
   private:
    static constexpr auto location = veiler::pegasus::semantic_actions::location;
    static constexpr auto omit = veiler::pegasus::semantic_actions::omit;
    static constexpr auto read = veiler::pegasus::read;
    static constexpr auto value = veiler::pegasus::semantic_actions::value;
    template<typename T>
    static constexpr auto lit(T&& t){return veiler::pegasus::lit(std::forward<T>(t));}
    template<typename T, typename U>
    static constexpr auto range(T&& t, U&& u){return veiler::pegasus::range(std::forward<T>(t), std::forward<U>(u));}
    static constexpr auto set_token(token_type t){
      return [t](auto&& loc, [[maybe_unused]] auto&&... unused){return std::make_pair(loc, t);};
    }
    AUTO_RULE(rule_inner_include, veiler::pegasus::transient(
           rules.header_name
        |  rules.preprocessing_token
        ))
    AUTO_RULE(header_name, veiler::pegasus::transient(
        (  lit('<') >> +rules.h_char >> lit('>')
        |  lit('"') >> +rules.q_char >> lit('"')
        )[location][set_token(token_type::header_name)]
        ))
    AUTO_RULE(h_char, veiler::pegasus::transient(
           omit[read - lit('\n') - lit('>')]
        ))
    AUTO_RULE(q_char, veiler::pegasus::transient(
           omit[read - lit('\n') - lit('"')]
        ))
    AUTO_RULE(preprocessing_token, veiler::pegasus::transient(
           rules.white_spaces[location][set_token(token_type::white_space)]
        |  lit('\n')[location][set_token(token_type::eol)]
        |  rules.pp_number[location][set_token(token_type::pp_number)]
        |  rules.punctuators
        |  rules.character_literal[location][set_token(token_type::character_literal)]
        |  rules.string_literal
        |  rules.directive
        |  (lit("true") >> !rules.identifier)[location][set_token(token_type::identifier_true)]
        |  (lit("false") >> !rules.identifier)[location][set_token(token_type::identifier_false)]
        |  (lit("defined") >> !rules.identifier)[location][set_token(token_type::identifier_defined)]
        |  (lit("__TIME__") >> !rules.identifier)[location][set_token(token_type::identifier_time_)]
        |  (lit("__DATE__") >> !rules.identifier)[location][set_token(token_type::identifier_date_)]
        |  (lit("__FILE__") >> !rules.identifier)[location][set_token(token_type::identifier_file_)]
        |  (lit("__LINE__") >> !rules.identifier)[location][set_token(token_type::identifier_line_)]
        |  (lit("_Pragma") >> !rules.identifier)[location][set_token(token_type::identifier_pragma_op)]
        |  (lit("__has_include") >> !rules.identifier)[location][set_token(token_type::identifier_has_include)]
        |  rules.identifier[location][set_token(token_type::identifier)]
        |  read[location][set_token(token_type::unclassified_character)]
        ))
    AUTO_RULE(punctuators, veiler::pegasus::transient(
           lit("%:%:"  )[location][set_token(token_type::punctuator_hashhash)]
        |  lit("..."   )[location][set_token(token_type::punctuator_ellipsis)]
        |  lit("<<="   )[location][set_token(token_type::punctuator)]
        |  lit(">>="   )[location][set_token(token_type::punctuator)]
        |  lit("->*"   )[location][set_token(token_type::punctuator)]//C++
        |  lit(".*"    )[location][set_token(token_type::punctuator)]//C++
        |  lit("%:"    )[location][set_token(token_type::punctuator_hash)]
        |  lit("<:"    )[location][set_token(token_type::punctuator)]
        |  lit(":>"    )[location][set_token(token_type::punctuator)]
        |  lit("<%"    )[location][set_token(token_type::punctuator)]
        |  lit("%>"    )[location][set_token(token_type::punctuator)]
        |  lit("##"    )[location][set_token(token_type::punctuator_hashhash)]
        |  lit("<<"    )[location][set_token(token_type::punctuator_left_shift)]
        |  lit(">>"    )[location][set_token(token_type::punctuator_right_shift)]
        |  lit("->"    )[location][set_token(token_type::punctuator)]
        |  lit("++"    )[location][set_token(token_type::punctuator)]
        |  lit("--"    )[location][set_token(token_type::punctuator)]
        |  lit("<="    )[location][set_token(token_type::punctuator_less_equal)]
        |  lit(">="    )[location][set_token(token_type::punctuator_greater_equal)]
        |  lit("=="    )[location][set_token(token_type::punctuator_equalequal)]
        |  lit("!="    )[location][set_token(token_type::punctuator_not_equal)]
        |  lit("&&"    )[location][set_token(token_type::punctuator_logical_and)]
        |  lit("||"    )[location][set_token(token_type::punctuator_logical_or)]
        |  lit("*="    )[location][set_token(token_type::punctuator)]
        |  lit("/="    )[location][set_token(token_type::punctuator)]
        |  lit("%="    )[location][set_token(token_type::punctuator)]
        |  lit("+="    )[location][set_token(token_type::punctuator)]
        |  lit("-="    )[location][set_token(token_type::punctuator)]
        |  lit("&="    )[location][set_token(token_type::punctuator)]
        |  lit("^="    )[location][set_token(token_type::punctuator)]
        |  lit("|="    )[location][set_token(token_type::punctuator)]
        |  lit("::"    )[location][set_token(token_type::punctuator)]//C++
        |  lit('#'     )[location][set_token(token_type::punctuator_hash)]
        |  lit('['     )[location][set_token(token_type::punctuator)]
        |  lit(']'     )[location][set_token(token_type::punctuator)]
        |  lit('('     )[location][set_token(token_type::punctuator_left_parenthesis)]
        |  lit(')'     )[location][set_token(token_type::punctuator_right_parenthesis)]
        |  lit('{'     )[location][set_token(token_type::punctuator)]
        |  lit('}'     )[location][set_token(token_type::punctuator)]
        |  lit('.'     )[location][set_token(token_type::punctuator)]
        |  lit('&'     )[location][set_token(token_type::punctuator_ampersand)]
        |  lit('*'     )[location][set_token(token_type::punctuator_asterisk)]
        |  lit('+'     )[location][set_token(token_type::punctuator_plus)]
        |  lit('-'     )[location][set_token(token_type::punctuator_minus)]
        |  lit('~'     )[location][set_token(token_type::punctuator_bitwise_not)]
        |  lit('!'     )[location][set_token(token_type::punctuator_logical_not)]
        |  lit('/'     )[location][set_token(token_type::punctuator_division)]
        |  lit('%'     )[location][set_token(token_type::punctuator_modulo)]
        |  lit('<'     )[location][set_token(token_type::punctuator_less)]
        |  lit('>'     )[location][set_token(token_type::punctuator_greater)]
        |  lit('^'     )[location][set_token(token_type::punctuator_bitwise_xor)]
        |  lit('|'     )[location][set_token(token_type::punctuator_bitwise_or)]
        |  lit('?'     )[location][set_token(token_type::punctuator_question)]
        |  lit(':'     )[location][set_token(token_type::punctuator_colon)]
        |  lit(';'     )[location][set_token(token_type::punctuator)]
        |  lit('='     )[location][set_token(token_type::punctuator)]
        |  lit(','     )[location][set_token(token_type::punctuator_comma)]
        |  lit("bitand")[location][set_token(token_type::punctuator_ampersand)] >> omit[!rules.identifier]
        |  lit("and_eq")[location][set_token(token_type::punctuator)] >> omit[!rules.identifier]
        |  lit("xor_eq")[location][set_token(token_type::punctuator)] >> omit[!rules.identifier]
        |  lit("not_eq")[location][set_token(token_type::punctuator_not_equal)] >> omit[!rules.identifier]
        |  lit("bitor" )[location][set_token(token_type::punctuator_bitwise_or)] >> omit[!rules.identifier]
        |  lit("compl" )[location][set_token(token_type::punctuator_bitwise_not)] >> omit[!rules.identifier]
        |  lit("or_eq" )[location][set_token(token_type::punctuator)] >> omit[!rules.identifier]
        |  lit("and"   )[location][set_token(token_type::punctuator_logical_and)] >> omit[!rules.identifier]
        |  lit("xor"   )[location][set_token(token_type::punctuator_bitwise_xor)] >> omit[!rules.identifier]
        |  lit("not"   )[location][set_token(token_type::punctuator_logical_not)] >> omit[!rules.identifier]
        |  lit("or"    )[location][set_token(token_type::punctuator_logical_or)] >> omit[!rules.identifier]
        ))
    AUTO_RULE(pp_number, veiler::pegasus::transient(
           (  rules.digit
           |  lit('.') >> rules.digit
           )
        >> *( -lit('\'') >>  rules.digit
            | lit('e') >> rules.sign
            | lit('E') >> rules.sign
            | lit('p') >> rules.sign
            | lit('P') >> rules.sign
            | lit('.')
            | -lit('\'') >> rules.ident_nondigit
            )
        ))
    AUTO_RULE(character_literal, veiler::pegasus::transient(
           -( lit("u8")
            | rules.encoding_prefix
            )
        >> lit('\'')
        >> +rules.c_char
        >> lit('\'')
        >> -rules.identifier//UDL
        ))
    AUTO_RULE(encoding_prefix, veiler::pegasus::transient(
          lit('u')
        | lit('U')
        | lit('L')
        ))
    AUTO_RULE(c_char, veiler::pegasus::transient(
           ( read - lit('\'') - lit('\\') - lit('\n') )
        |  rules.escape_sequence
        ))
    AUTO_RULE(escape_sequence, veiler::pegasus::transient(
           lit('\\')
        >> (  lit('\'')
           |  lit('"')
           |  lit('?')
           |  lit('\\')
           |  lit('a')
           |  lit('b')
           |  lit('f')
           |  lit('n')
           |  lit('r')
           |  lit('t')
           |  lit('v')
           )
           |
           (  rules.octo >> rules.octo >> rules.octo
           |  rules.octo >> rules.octo
           |  rules.octo
           )
           |
           (  lit('x')
           >> +rules.hex
           )
        ))
    AUTO_RULE(string_literal, veiler::pegasus::transient(
         ( -( lit("u8")
            | rules.encoding_prefix
            )
        >> lit('"')
        >> *rules.s_char
        >> lit('"')
        >> -rules.identifier
         )[location][set_token(token_type::string_literal)]
         |
         ( -( lit("u8")
            | rules.encoding_prefix
            )
        >> lit("R\"")
        >> veiler::pegasus::filter([&rules](auto&& v, auto&& end, auto&&... args){
            const auto raw_string_literal_parser =
             (*rules.d_char)[location][([](auto&& loc, auto&&, auto&& delim, [[maybe_unused]] auto&&... unused){delim.assign(loc.begin(), loc.end());return veiler::pegasus::unit{};})]
          >> lit('(')
          >> *(veiler::pegasus::read
              - ( lit(')')
               >> veiler::pegasus::loop(
                   veiler::pegasus::eps[([](auto&&, auto&&, auto&& delim, [[maybe_unused]] auto&&... unused){return delim.size();})], 
                   rules.d_char[value])[veiler::pegasus::semantic_actions::change_container<std::string>]
                     [([](auto&& v, auto&& loc, auto&& delim, [[maybe_unused]] auto&&... unused)
                       ->veiler::expected<veiler::pegasus::unit, veiler::pegasus::parse_error<decltype(loc.begin())>>{
                         if(v != delim)return veiler::make_unexpected(veiler::pegasus::error_type::semantic_check_failed{"raw string literal: unmatch delimiter"});
                         else return veiler::pegasus::unit{};
                     })]
               >> lit('"')
                ))
          >> lit(')')
          >> veiler::pegasus::loop(
                   veiler::pegasus::eps[([](auto&&, auto&&, auto&& delim, [[maybe_unused]] auto&&... unused){return delim.size();})], 
                   rules.d_char)
          >> &lit('"');
            auto it = messer::get_raw(v);
            auto ret = raw_string_literal_parser[omit](it, messer::get_raw(end), args...);
            messer::get_raw(v) = it;
            ++v;
            return ret.valid();
             })
        >> -rules.identifier//UDL
         )[location][set_token(token_type::string_literal)]
        ))
    AUTO_RULE(s_char, veiler::pegasus::transient(
           ( read - lit('"') - lit('\\') - lit('\n') )
        |  rules.escape_sequence
        ))
    AUTO_RULE(d_char, veiler::pegasus::transient(
          read
        - lit(' ')
        - lit('(')
        - lit(')')
        - lit('\\')
        - lit('\t')
        - lit('\v')
        - lit('\f')
        - lit('\n')
        ))
    AUTO_RULE(directive, veiler::pegasus::transient(
        (  lit("include") >> &( rules.white_spaces
                              | lit('<')
                              | lit('"')
                              )
        )[location][set_token(token_type::identifier_include)]
        |
        (  lit("define")  >> &  rules.white_spaces
        )[location][set_token(token_type::identifier_define)]
        |
        (  lit("undef")   >> &  rules.white_spaces
        )[location][set_token(token_type::identifier_undef)]
        |
        (  lit("line")    >> &  rules.white_spaces
        )[location][set_token(token_type::identifier_line)]
        |
        (  lit("error")   >> &( rules.white_spaces
                              | lit('\n')
                              )
        )[location][set_token(token_type::identifier_error)]
        |
        (  lit("pragma")  >> &( rules.white_spaces
                              | lit('\n')
                              )
        )[location][set_token(token_type::identifier_pragma)]
        |
        (  lit("if")      >> &( rules.white_spaces
                              | lit('(')
                              )
        )[location][set_token(token_type::identifier_if)]
        |
        (  lit("ifdef")   >> &  rules.white_spaces
        )[location][set_token(token_type::identifier_ifdef)]
        |
        (  lit("ifndef")  >> &  rules.white_spaces
        )[location][set_token(token_type::identifier_ifndef)]
        |
        (  lit("elif")    >> &( rules.white_spaces
                              | lit('(')
                              )
        )[location][set_token(token_type::identifier_elif)]
        |
        (  lit("else")    >> &( rules.white_spaces
                              | lit('\n')
                              )
        )[location][set_token(token_type::identifier_else)]
        |
        (  lit("endif")   >> &( rules.white_spaces
                              | lit('\n')
                              )
        )[location][set_token(token_type::identifier_endif)]
        ))
    AUTO_RULE(identifier, veiler::pegasus::transient(
        (
           rules.ident_nondigit
        >> *( rules.ident_nondigit
            | rules.digit
            )
        )
        ))
    AUTO_RULE(white_spaces, veiler::pegasus::transient(
           +( lit(' ')
            | lit('\t')
            | lit("/*") >> *( read - lit("*/") ) >> lit("*/")
            | lit("//") >> *( read - lit('\n') )
            | lit('\v')
            | lit('\f')
            )
        ))
    RULE(ident_nondigit, char, veiler::pegasus::transient(
           rules.nondigit
        ))
    RULE(nondigit, char, veiler::pegasus::transient(
           range('A', 'Z')[value]
        |  range('a', 'z')[value]
        |  lit('_')[value]
        ))
    RULE(digit, char, veiler::pegasus::transient(
           range('0', '9')[value]
        ))
    RULE(hex, char, veiler::pegasus::transient(
           rules.digit
        |  range('A', 'F')[value]
        |  range('a', 'f')[value]
        ))
    RULE(octo, char, veiler::pegasus::transient(
           range('0', '7')[value]
        ))
    RULE(sign, char, veiler::pegasus::transient(
           lit('+')[value]
        |  lit('-')[value]
        ))
  };
 public:
  class value_type : public token<std::string>{
    using parent = token<std::string>;
    annotation_type data;
   public:
    value_type(parent&& tk, const annotation_type& anno):parent{std::move(tk)}, data{anno}{}
    std::string_view filename()const{return data.filename;}
    std::size_t line()const{return data.line;}
    std::size_t column()const{return data.column;}
    const annotation_type& annotation()const{return data;}
    annotation_type& annotation(){return data;}
    value_type& filename(std::string_view sv){data.filename = std::move(sv); return *this;}
    value_type& line(std::size_t ln){data.line = ln; return *this;}
    friend std::ostream& operator<<(std::ostream& os, const value_type& v){return os << *static_cast<const parent*>(&v);}
  };
 private:
  template<typename T>
  class lexer_iterator_impl{
    using impl = typename T::iterator;
    impl it;
    impl end;
    enum class status{
      first,
      pp_directive,
      include,
      none
    }st = status::first;
    token_type tkt = token_type::eol;
    impl beg;
    bool result;
   public:
    using value_type = phase3_t::value_type;
    using iterator = filter_iterator<lexer_iterator_impl<T>>;
    lexer_iterator_impl(const impl& b, const impl& e):it(b), end(e), beg(b), result(b != e){}
    lexer_iterator_impl(const lexer_iterator_impl&) = default;
    lexer_iterator_impl& operator=(const lexer_iterator_impl&) = default;
    value_type dereference()const{
      if(!result)
        throw std::runtime_error("can't dereference it");
      const auto& i = get_raw(beg);
      const auto anno = annotation_type{i.get_filename(), i.get_line(), i.get_column()};
      if(tkt != token_type::string_literal || *beg == '"')
        return value_type{token<std::string>{std::string{beg, it}, tkt}, anno};
      auto itt = beg;
      while(std::next(itt) != end && *std::next(itt) != '"')
        ++itt;
      if(*itt != 'R')
        return value_type{token<std::string>{std::string{beg, it}, tkt}, anno};
      ++itt;
      auto edit = itt;
      auto edit_raw = messer::get_raw(edit);
      std::string delimiter;
      for(auto itr = std::next(edit_raw); *itr != '('; ++itr)
        delimiter.push_back(*itr);
      std::advance(edit_raw, delimiter.size());
      while(true){
        while(*edit_raw++ != ')');
        auto itr = edit_raw;
        for(auto&& x : delimiter){
          if(*itr++ != x)
            break;
        }
        if(*itr == '"'){
          messer::get_raw(edit) = itr;
          break;
        }
      }
      return value_type{token<std::string>{std::string{beg, itt} + std::string{messer::get_raw(itt).get_inner(), messer::get_raw(edit).get_inner()} + std::string{edit, it} , tkt}, anno};
    }
    void next(){
      if(!result)
        return;
      beg = it;
      auto ret = (st == status::include ? 
        lexer::inner_include()(it, end, std::string{}):
        lexer::entrypoint   ()(it, end, std::string{}));
      result = ret && it != beg;
      if(!result)
        return;
      tkt = ret--->second;
      if(st == status::first && tkt == token_type::punctuator_hash)
        st = status::pp_directive;
      else if(st == status::pp_directive && tkt == token_type::identifier_include)
        st = status::include;
      else if(tkt == token_type::eol)
        st = status::first;
      else if(tkt != token_type::white_space)
        st = status::none;
    }
    bool is_equal(const lexer_iterator_impl& other)const noexcept{return result == other.result && it == other.it;}
          auto& get_raw_iterator()     {return it;}
    const auto& get_raw_iterator()const{return it;}
  };
  template<typename T>
  using lexer_range = filter_range<T, lexer_iterator_impl>;
 public:
  template<typename T>
  friend constexpr auto operator|(T&& t, const phase3_t&)noexcept{
    return lexer_range<T>{std::forward<T>(t)};
  }
};

}

namespace veiler{

template<>
struct hash<messer::phase3_t::value_type> : hash<std::underlying_type_t<messer::token_type>>, hash<std::string>, hash<std::string_view>,  hash<std::size_t>{
  constexpr hash() = default;
  constexpr hash(const hash&) = default;
  constexpr hash(hash&&) = default;
  ~hash() = default;
  hash& operator=(const hash&) = default;
  hash& operator=(hash&&) = default;
  using result_type = std::size_t;
  using argument_type = messer::phase3_t::value_type;
  std::size_t operator()(const argument_type& key)const noexcept{
    return detail::fool::hash_combine(hash<std::underlying_type_t<messer::token_type>>::operator()(static_cast<std::underlying_type_t<messer::token_type>>(key.type())), hash<std::string>::operator()(key.get()), hash<std::string_view>::operator()(key.filename()), hash<std::size_t>::operator()(key.line()), hash<std::size_t>::operator()(key.column()));
  }
};

}

namespace veiler::pegasus{

template<>
struct member_accessor<std::string_view, messer::phase3_t::value_type>{
  static const std::string& access(const messer::phase3_t::value_type& t){return t.get();}
};
template<>
struct member_accessor<std::string, messer::phase3_t::value_type>{
  static const std::string& access(const messer::phase3_t::value_type& t){return t.get();}
};
template<>
struct member_accessor<const char*, messer::phase3_t::value_type>{
  static const std::string& access(const messer::phase3_t::value_type& t){return t.get();}
};
template<>
struct member_accessor<messer::token_type, messer::phase3_t::value_type>{
  static messer::token_type access(const messer::phase3_t::value_type& t){return t.type();}
};

}

#include<iostream>
#include<vector>
#include<list>
namespace messer{

template<typename T, typename U>
inline bool tokens_equal(const T& t, const U& u){
  if(t.size() != u.size())
    return false;
  for(auto t_it = t.begin(), u_it = u.begin(); t_it != t.end(); ++t_it, ++u_it)
    if(t_it->get() != u_it->get())
      return false;
  return true;
}

template<typename T>
inline std::vector<T> vector_product(std::vector<T> x, std::vector<T> y){
  std::sort(x.begin(), x.end());
  std::sort(y.begin(), y.end());
  std::vector<T> product;
  product.reserve(x.size() + y.size());
  std::set_intersection(x.begin(), x.end(), y.begin(), y.end(), std::back_inserter(product));
  return product;
}

namespace detail{

template<typename T>
struct list_replace_1{
  output_range<typename std::list<T>::const_iterator> r;
  std::list<T> data;
  output_range<typename std::list<T>::iterator> operator()(std::list<T>& l){
    if(data.empty()){
      const auto it = l.erase(r.begin(), r.end());
      return output_range<typename std::list<T>::iterator>{it, it};
    }
    const auto inserted_begin = data.begin();
    l.splice(r.begin(), std::move(data));
    return output_range<typename std::list<T>::iterator>{inserted_begin, l.erase(r.begin(), r.end())};
  }
  friend output_range<typename std::list<T>::iterator> operator|(std::list<T>& l, list_replace_1&& rep){return std::move(rep)(l);}
  list_replace_1(output_range<typename std::list<T>::const_iterator> r, std::list<T>&& data):r{std::move(r)}, data{std::move(data)}{}
  list_replace_1(output_range<typename std::list<T>::iterator> r, std::list<T>&& data):r{r.begin(), r.end()}, data{std::move(data)}{}
};

template<typename T>
struct list_replace_2{
  output_range<typename std::list<T>::iterator> r;
  T data;
  output_range<typename std::list<T>::iterator> operator()(std::list<T>& l){
    const auto inserted_begin = l.insert(r.begin(), std::move(data));
    return {inserted_begin, l.erase(r.begin(), r.end())};
  }
  friend output_range<typename std::list<T>::iterator> operator|(std::list<T>& l, list_replace_2&& rep){return std::move(rep)(l);}
};

template<typename T, typename U>
struct list_replace_3{
  output_range<typename std::list<T>::iterator> r;
  output_range<U> data;
  output_range<typename std::list<T>::iterator> operator()(std::list<T>& l){
    const auto inserted_begin = l.insert(r.begin(), data.begin(), data.end());
    return {inserted_begin, l.erase(r.begin(), r.end())};
  }
  friend output_range<typename std::list<T>::iterator> operator|(std::list<T>& l, list_replace_3&& rep){return std::move(rep)(l);}
};

}

template<typename T>
inline detail::list_replace_1<typename std::iterator_traits<T>::value_type> replacer(const T& rb, const T& re, std::list<typename std::iterator_traits<T>::value_type> l){return detail::list_replace_1<typename std::iterator_traits<T>::value_type>(output_range<T>{rb, re}, std::move(l));}
template<typename T>
inline detail::list_replace_2<typename std::iterator_traits<T>::value_type> replacer(const T& rb, const T& re, typename std::iterator_traits<T>::value_type t){return {output_range<T>{rb, re}, std::move(t)};}
template<typename T, typename U>
inline detail::list_replace_3<typename std::iterator_traits<T>::value_type, U> replacer(const T& rb, const T& re, const output_range<U>& r2){return {output_range<T>{rb, re}, r2};}
template<typename T, typename U, typename Y>
inline detail::list_replace_3<typename std::iterator_traits<T>::value_type, std::common_type_t<U, Y>> replacer(const T& rb, const T& re, const U& b, const Y& e){return replacer(rb, re, output_range<std::common_type_t<U, Y>>{b, e});}

inline std::string escape(std::string_view str){
  std::string ret;
  for(auto&& x : str)
    switch(x){
      case '\t':ret += "\\t";break;
      case '\'':ret += "\\\'";break;
      case '"': ret += "\\\"";break;
      case '\\':ret += "\\\\";break;
      case '\a':ret += "\\a";break;
      case '\b':ret += "\\b";break;
      case '\f':ret += "\\f";break;
      case '\n':ret += "\\n";break;
      case '\r':ret += "\\r";break;
      case '\v':ret += "\\v";break;
      default:  ret += x;break;
    }
  return ret;
}

inline std::string unescape(std::string_view str){
  static constexpr auto oct = veiler::pegasus::filter([](auto&& x, auto&&){return '0' <= *x && *x <= '8';})[([](auto&& x, [[maybe_unused]] auto&&... unused)->int{return *x - '0';})] >> veiler::pegasus::read[veiler::pegasus::semantic_actions::omit];
  static constexpr auto hex = veiler::pegasus::filter([](auto&& x, auto&&){return '0' <= *x && *x <= '9';})[([](auto&& x, [[maybe_unused]] auto&&... unused)->int{return *x - '0';})] >> veiler::pegasus::read[veiler::pegasus::semantic_actions::omit]
                            | veiler::pegasus::filter([](auto&& x, auto&&){return 'a' <= *x && *x <= 'f';})[([](auto&& x, [[maybe_unused]] auto&&... unused)->int{return *x - 'a'+10;})] >> veiler::pegasus::read[veiler::pegasus::semantic_actions::omit]
                            | veiler::pegasus::filter([](auto&& x, auto&&){return 'A' <= *x && *x <= 'F';})[([](auto&& x, [[maybe_unused]] auto&&... unused)->int{return *x - 'A'+10;})] >> veiler::pegasus::read[veiler::pegasus::semantic_actions::omit];
  std::string ret;
  ret.reserve(str.size());
  for(auto it = str.begin(); it != str.end();)
    if(*it != '\\')
      ret += *it++;
    else if(it+1 != str.end())switch(*(it+1)){
      case 't': ret += '\t';it += 2;break;
      case '\'':ret += '\'';it += 2;break;
      case '"':ret += '"';it += 2;break;
      case '\\':ret += '\\';it += 2;break;
      case '?': ret += '?';it += 2;break;
      case 'a': ret += '\a';it += 2;break;
      case 'b': ret += '\b';it += 2;break;
      case 'f': ret += '\f';it += 2;break;
      case 'n': ret += '\n';it += 2;break;
      case 'r': ret += '\r';it += 2;break;
      case 'v': ret += '\v';it += 2;break;
      case 'x':{
        int t = 0;
        auto itt = it+2;
        if(auto h = hex(itt, str.end()))
          t = t*16+*h;
        else
          throw std::runtime_error("unescape: invalid \\x");
        while(auto h = hex(itt, str.end()))
          t = t*16+*h;
        ret.push_back(static_cast<std::string::value_type>(t));
        it = itt;
      }break;
      default:{
        int t = 0;
        auto itt = it+2;
        if(auto h = oct(itt, str.end()))
          t = t*8+*h;
        else
          throw std::runtime_error("unescape: invalid \\");
        if(auto h = oct(itt, str.end())){
          t = t*8+*h;
           if(auto h = oct(itt, str.end()))
             t = t*8+*h;
        }
        ret.push_back(static_cast<std::string::value_type>(t));
        it = itt;
      }
    }
  return ret;
}

template<typename T>
inline std::string stringizer(const output_range<T>& r){
  std::string str;
  for(auto&& x : r)
    if(x.type() == token_type::white_space
    || x.type() == token_type::eol)
      if(str.back() == ' ')
        continue;
      else
        str += ' ';
    else
      str += escape(x.get());
  return str;
}
template<typename T>
inline std::string stringizer(const T& b, const T& e){
  return stringizer(output_range<T>{b, e});
}

struct string_literal{
  std::string str;
  std::string suffix;//for UDL
  enum class prefix{none, L, u, U, u8} is;
  template<typename T>
  static std::optional<string_literal> destringize(T&& token){
    assert(token.type() == token_type::string_literal);
    auto it = token.get().cbegin();
    auto end = token.get().cend();
    prefix is = prefix::none;
    switch(*it){
    case 'L':
      is = prefix::L;
      ++it;
      break;
    case 'u':
      if(*(it+1) == '8')
        is = prefix::u8, ++it;
      else
        is = prefix::u;
      ++it;
      break;
    case 'U':
      is = prefix::U;
      ++it;
      break;
    default:;
    }
    const bool is_raw = *it == 'R';
    if(is_raw)++it;
    if(*it++ != '"')
      return std::nullopt;
    if(!is_raw){
      while(*--end != '"');
      return string_literal{unescape(std::string_view{&*it, static_cast<std::string_view::size_type>(end-it)}), std::string(end+1, token.get().cend()), is};
    }
    std::string delim;
    while(*it != '(')
      delim.push_back(*it++);
    ++it;
    end = it;
    while(end != token.get().cend()){
      if(*end == ')'){
        std::string_view str{&*(end+1), delim.size()};
        if(str == delim && *(end+1+delim.size()) == '"')
          return string_literal{std::string(it, end), std::string(end+2+delim.size(), token.get().cend()), is};
      }
      ++end;
    }
    return std::nullopt;
  }
  phase3_t::value_type stringize(const annotation_type& anno)const{
    std::string s;
    switch(is){
    case prefix::none:break;
    case prefix::L:s.push_back('L');break;
    case prefix::u:s.push_back('u');break;
    case prefix::U:s.push_back('U');break;
    case prefix::u8:s.push_back('u');s.push_back('8');break;
    }
    s.push_back('"');
    s += escape(str);
    s.push_back('"');
    s += suffix;
    return phase3_t::value_type{{std::move(s), token_type::string_literal}, anno};
  }
  string_literal& operator+=(const string_literal& rhs){
    if(is == prefix::none)
      is = rhs.is;
    else if(rhs.is != prefix::none && is != rhs.is)
      throw std::runtime_error("concat string literals failed (unmatch prefixes)");
    if(suffix.empty())
      suffix = rhs.suffix;
    else if(!rhs.suffix.empty() && suffix != rhs.suffix)
      throw std::runtime_error("concat string literals failed (unmatch user-defined literals)");
    str += rhs.str;
    return *this;
  }
};

template<typename T>
struct iterator_hasher : private std::hash<typename std::iterator_traits<T>::pointer>, std::hash<typename std::iterator_traits<T>::value_type>{
  iterator_hasher() = default;
  iterator_hasher(const iterator_hasher&) = default;
  iterator_hasher(iterator_hasher&&) = default;
  ~iterator_hasher() = default;
  iterator_hasher& operator=(const iterator_hasher&) = default;
  iterator_hasher& operator=(iterator_hasher&&) = default;
  std::size_t operator()(T key)const{return veiler::detail::fool::hash_combine(static_cast<const std::hash<typename std::iterator_traits<T>::pointer>*>(this)->operator()(std::addressof(*key)), static_cast<const std::hash<typename std::iterator_traits<T>::value_type>*>(this)->operator()(*key));}
  using result_type = std::size_t;
  using argument_type = T;
};

}

namespace std{

template<>
struct hash<messer::phase3_t::value_type> : hash<string>{
  using result_type = std::size_t;
  using argument_type = messer::phase3_t::value_type;
  std::size_t operator()(const argument_type& key)const{return static_cast<const hash<string>*>(this)->operator()(key.get());}
};

}

#include<sstream>
#include<numeric>

namespace messer{

static std::list<phase3_t::value_type> phase6(std::list<phase3_t::value_type>);

class phase4_t{
 public:
  using token_t = phase3_t::value_type;

  using filepath = std::filesystem::path;
  std::vector<filepath> system_include_dir;
  std::vector<filepath>        include_dir;
  std::unordered_map<std::string, std::list<token_t>> files;
  std::unordered_map<std::string, output_range<std::list<token_t>::const_iterator>> objects;
  struct func_t{
    int arg_num;
    std::vector<int> arg_index;
    output_range<std::list<token_t>::const_iterator> dst;
  };
  std::unordered_map<std::string, func_t> functions;
  struct pp_state{
    std::list<token_t>& list;
    std::unordered_map<std::list<token_t>::const_iterator, std::vector<std::string>, iterator_hasher<std::list<token_t>::const_iterator>> replaced;
  };
  template<typename T>
  static constexpr auto _(T&& t){return veiler::pegasus::lit(std::forward<T>(t))[veiler::pegasus::semantic_actions::omit];}
  struct passed_identity_t{
    constexpr passed_identity_t() = default;
    template<typename T>
    constexpr bool operator()(T&&)const{return true;}
  }static constexpr passed_identity = {};
  struct eval_macro_t{
    constexpr eval_macro_t() = default;
    template<typename F>
    static auto en_us_utf8_locale_for_time(F&& f){
      struct raii{
        std::string saved_locale;
        raii() : saved_locale{std::setlocale(LC_TIME, nullptr)}{
          std::setlocale(LC_TIME, "en_us.UTF-8");
        }
        ~raii(){
          std::setlocale(LC_TIME, saved_locale.c_str());
        }
      }_;
      return f();
    }
    static std::string format_time_point(const std::chrono::system_clock::time_point& time, const char* strftime_format, std::string_view expected_format){
      return en_us_utf8_locale_for_time([&]{
        auto t = std::chrono::system_clock::to_time_t(time);
        std::string str{expected_format};
        str.push_back('.');
        std::strftime(str.data(), str.size(), strftime_format, std::localtime(&t));
        str.pop_back();
        return str;
      });
    }
    static std::string time(){
      return format_time_point(std::chrono::system_clock::now(), "%H:%M:%S", "hh:mm:ss");
    }
    static std::string date(){
      return format_time_point(std::chrono::system_clock::now(), "%b %e %Y", "Mmm dd yyyy");
    }
    template<typename T, typename U>
    static auto cat_token(T&& prev, U&& next){
      if(prev.type() == token_type::empty)
        return next;
      if(next.type() == token_type::empty)
        return prev;
      auto str = prev.get() + next.get();
      auto range = str | annotation{prev.annotation()} | phase3_t{};
      if(std::distance(std::next(range.begin()), range.end()) != 1){
        for(auto&& x : range)
          std::cout << x.type() << " : " << x.get() << std::endl;
        throw std::runtime_error("operator ## makes invalid token");
      }
      auto t = *std::next(range.begin());
      if(t.type() == token_type::punctuator_hash || t.type() == token_type::punctuator_hashhash)
        return token_t{token<std::string>{t.get(), token_type::punctuator}, t.annotation()};
      return t;
    }
    template<typename Iterator, typename F>
    static auto search_(Iterator it, Iterator sentinel, F&& f){
      do{
        if(it == sentinel)
          throw std::runtime_error("search_ failed");
        f(it);
      }while(is_white_spaces(it->type()));
      return it;
    }
    template<typename Hash>
    static auto apply_cat(Hash&& hashhash, pp_state& pps){
      static auto search = [](auto it, auto sentinel, auto&& f){
        try{
          return search_(std::move(it), std::move(sentinel), f);
        }catch(std::runtime_error&){
          throw std::runtime_error("operator ## can't receive parameter(search_parameter reach edge)");
        }
      };
      const auto prev = search(hashhash, pps.list.begin(), [](auto&& it){--it;});
      auto next = search(hashhash, pps.list.end(), [](auto&& it){++it;});
      const auto replaced_pos = pps.list.insert(prev, cat_token(*prev, *next));
      ++next;
      for(auto it = prev; it != next; ++it)
        pps.replaced.erase(it);
      pps.list.erase(prev, next);
      return replaced_pos;
    }
    template<typename Passed, typename Iterator, typename End, typename Yield>
    auto object_macro_replace(const decltype(objects)::const_iterator& object_it, Passed&& passed, const phase4_t& state, pp_state& tmp_state, Iterator&& it, const End& end, Yield&& yield)const{
      auto check_recur = tmp_state.replaced.find(it);
      if(check_recur != tmp_state.replaced.end())
        for(auto&& x : check_recur->second)
          if(it->get() == x)
            return passed(*it++);
      std::list<token_t> copy(object_it->second.begin(), object_it->second.end());
      for(auto&& x : copy)
        x.annotation() = it->annotation();
      pp_state copy_state{copy, std::move(tmp_state.replaced)};
      yield(state, copy_state, copy.begin(), {output_range<std::list<token_t>::const_iterator>{copy.begin(), copy.end()}, output_range<std::list<token_t>::const_iterator>{std::next(it), end}});
      for(auto it_ = std::next(copy.begin()), end_ = copy.end(); it_ != end_; ++it_)
        if(it_->type() == token_type::punctuator_hashhash)
          it_ = apply_cat(it_, copy_state),
          yield(state, copy_state, std::next(it_), {output_range<std::list<token_t>::const_iterator>{copy.begin(), copy.end()}, output_range<std::list<token_t>::const_iterator>{std::next(it), end}});
      for(auto it_ = copy.begin(), end_ = copy.end(); it_ != end_; ++it_){
        if(check_recur != tmp_state.replaced.end())
          copy_state.replaced[it_] = check_recur->second;
        copy_state.replaced[it_].emplace_back(it->get());
      }
      tmp_state.replaced = std::move(copy_state.replaced);
      auto replaced = (tmp_state.list|replacer(it, std::next(it), std::move(copy)));
      it = replaced.begin();
      return true;
    }
    template<typename Passed, typename Iterator, typename End>
    auto save_defined_argument(Passed&& passed, Iterator&& it, const End& end)const{
      static constexpr auto identifier = veiler::pegasus::filter([](auto&& v, [[maybe_unused]] auto&&... unused){return is_identifier(veiler::pegasus::member_access<token_type>(*v++));});
      static constexpr auto arg_parser = (
           veiler::pegasus::read //token_type::identifier_defined
        >> ( ( _(token_type::punctuator_left_parenthesis)
            >> identifier
            >> _(token_type::punctuator_right_parenthesis)
             )
             | identifier
           )
      ).with_skipper(*veiler::pegasus::filter([](auto&& it, [[maybe_unused]] auto&&... unused){return is_white_spaces(veiler::pegasus::member_access<token_type>(*it++));}));
      auto parse_result = arg_parser[veiler::pegasus::semantic_actions::location](it, end);
      if(!parse_result)
        return false;
      for(auto&& x : *parse_result)
        passed(x);
      return true;
    }
    template<typename Passed, typename Iterator, typename End>
    auto save_has_include_argument(Passed&& passed, Iterator&& it, const End& end)const{
      static constexpr auto arg_parser = (
           veiler::pegasus::read //token_type::identifier_has_include
        >> _(token_type::punctuator_left_parenthesis)
        >> ( _(token_type::header_name)
           | _(token_type::string_literal)
           | veiler::pegasus::lexeme[_(token_type::punctuator_less) >> *(veiler::pegasus::read - _(token_type::punctuator_greater)) >> _(token_type::punctuator_greater)]
           )
        >> _(token_type::punctuator_right_parenthesis)
      ).with_skipper(*veiler::pegasus::filter([](auto&& it, [[maybe_unused]] auto&&... unused){return is_white_spaces(veiler::pegasus::member_access<token_type>(*it++));}));
      auto parse_result = arg_parser[veiler::pegasus::semantic_actions::location](it, end);
      if(!parse_result)
        return false;
      for(auto&& x : *parse_result)
        passed(x);
      return true;
    }
    template<bool InArithmeticEvaluation = false, typename Self, typename Passed, typename Iterator, typename End, typename Yield>
    auto operator()(Self&& self, Passed&& passed, const phase4_t& state, pp_state& tmp_state, Iterator&& it, const End& end, Yield&& yield)const{
      if(it == end)
        return false;
      if(!is_identifier(it->type()))
        return passed(*it++);
      if constexpr(InArithmeticEvaluation){
        if(it->type() == token_type::identifier_defined)
          return save_defined_argument(std::forward<Passed>(passed), std::forward<Iterator>(it), end);
        if(it->type() == token_type::identifier_has_include)
          return save_has_include_argument(std::forward<Passed>(passed), std::forward<Iterator>(it), end);
      }
      bool is_pragma_op = false;
      {
        const auto make_token_and_pass = [&](std::string&& str, token_type tt = token_type::string_literal){
          std::list<token_t> list{phase3_t::value_type{{std::move(str), tt}, it->annotation()}};
          yield(state, tmp_state, it, {output_range<std::list<token_t>::const_iterator>{list.begin(), list.end()}, {std::next(it), end}});
          it = (tmp_state.list|replacer(it, std::next(it), std::move(list))).begin();
          return true;
        };
        switch(it->type()){
        case token_type::identifier_time_:
          return make_token_and_pass('"'+time()+'"');
        case token_type::identifier_date_:
          return make_token_and_pass('"'+date()+'"');
        case token_type::identifier_file_:
          return make_token_and_pass('"'+escape(it->filename())+'"');
        case token_type::identifier_line_:
          return make_token_and_pass(std::to_string(it->line()), token_type::pp_number);
        case token_type::identifier_pragma_op:
          is_pragma_op = true;
          [[fallthrough]];
        default:
          break;
        }
      }
      {
        auto object_it = state.objects.find(it->get());
        if(object_it != state.objects.end())
          return object_macro_replace(object_it, std::forward<Passed>(passed), state, tmp_state, std::forward<Iterator>(it), end, std::forward<Yield>(yield));
      }
      constexpr auto white_spaces = veiler::pegasus::filter([](auto&& it, [[maybe_unused]] auto&&... unused){return is_white_spaces(veiler::pegasus::member_access<token_type>(*it++));})[veiler::pegasus::semantic_actions::omit];
      struct arg_parser_data{
        using type = std::decay_t<Iterator>;
        std::optional<type> first;
        std::optional<type> last;
        unsigned long nest_count;
      };
      auto not_arg = _(token_type::punctuator_comma) | _(token_type::punctuator_right_parenthesis);
      auto arg_parser = *white_spaces
                     >> ( +veiler::pegasus::freeze
                          (  (  _(token_type::punctuator_left_parenthesis) >> *white_spaces
                             >> *( _(token_type::punctuator_comma) >> *white_spaces
                                 | veiler::pegasus::self<veiler::pegasus::unit>[veiler::pegasus::semantic_actions::omit] >> *white_spaces
                                 )
                             >> _(token_type::punctuator_right_parenthesis) >> *white_spaces
                             )
                             |
                             ( veiler::pegasus::read - not_arg - _(token_type::punctuator_left_parenthesis))[veiler::pegasus::semantic_actions::omit]
                          )
                          |
                          &not_arg//empty token
                        )
                        ;
      auto arg_parser_registrar = [](auto&& first, auto&& loc){
        auto ret = output_range<std::list<token_t>::const_iterator>{first, loc.end()};
        return ret;
      };
      auto args_parser =  _(token_type::punctuator_left_parenthesis)
                       >> ( (*white_spaces)[([](auto&&, auto&& loc){return loc.end();})]
                         >> arg_parser[veiler::pegasus::semantic_actions::omit]
                          )[arg_parser_registrar] % _(token_type::punctuator_comma)
                       >> _(token_type::punctuator_right_parenthesis);
      auto f = state.functions.find(it->get());
      if(f == state.functions.end() && !is_pragma_op)
        return passed(*it++);
      std::vector<output_range<std::list<token_t>::const_iterator>> args;
      auto arg_it = [&]{
        try{
          return search_(it, end,[](auto&& t){++t;});
        }catch(std::runtime_error& e){
          return end;
        }
      }();
      if(arg_it == end)
        return passed(*it++);
      if(!(&_(token_type::punctuator_left_parenthesis))(arg_it, end))
        return passed(*it++);
      if(auto parsed_result = args_parser(arg_it, end))
        args = *parsed_result;
      else
        throw std::runtime_error(std::string{it->filename()} + ':' + std::to_string(it->line()) + ':' + std::to_string(it->column()) + ": error: ')' does not exist");
      if(is_pragma_op){
        if(args.size() != 1)
          return false;
        {
          auto it = args[0].begin();
          ++it;
          if(it != args[0].end())
            return false;
        }
        if(args[0].begin()->type() != token_type::string_literal)
          return false;
        std::string scratch = "#pragma " + string_literal::destringize(*args[0].begin())->str;
        auto range = scratch | annotation{"<pragma operator scratch>"} | phase1_t{} | phase2_t{} | phase3_t{};
        std::list<typename decltype(range.begin())::value_type> tokens(range.begin(), range.end());
        override_annotate oa{};
        const_cast<phase4_t&>(state).eval(tokens, output_range<std::list<typename decltype(range.begin())::value_type>::const_iterator>{tokens.cbegin(), tokens.cend()}, oa, std::filesystem::current_path(), false, std::cout);
        it = arg_it;
        return true;
      }
      if(f->second.arg_num > 0 && f->second.arg_num != static_cast<int>(args.size()))
        return false;
      //if argument list is (...) /*f->second.arg_num == -1*/, we can pass no arguments
      //but (a, b, ...), we need 3 or more arguments(can't pass 2 arguments)
      //GCC and clang can pass 2 args
      static constexpr auto allow_pass_no_arg_to_variadic_param = false;
      if(f->second.arg_num < -1 && -f->second.arg_num - (allow_pass_no_arg_to_variadic_param ? 1 : 0) > static_cast<int>(args.size()))
        return false;
      if(allow_pass_no_arg_to_variadic_param && f->second.arg_num < -1 && -f->second.arg_num - 1 == static_cast<int>(args.size()))
        args.emplace_back(output_range<std::list<token_t>::const_iterator>{args.back().end(), args.back().end()});
      if(f->second.arg_num == -1 && static_cast<int>(args.size()) == 0)
        args.emplace_back(output_range<std::list<token_t>::const_iterator>{it, it});
      bool first = true;
      std::vector<std::string> recur;
      for(auto it_ = it; it_ != arg_it && (first || !recur.empty()); ++it_){
        auto rit = tmp_state.replaced.find(it_);
        if(rit != tmp_state.replaced.end()){
          if(first)
            recur = rit->second, first = false;
          else
            recur = vector_product(std::move(recur), rit->second);
        }
        else
          recur.clear(), first = false;
      }
      for(auto&& x : recur){
        if(x == it->get()){
          return passed(*it++);
        }
      }
      {
        auto rit = tmp_state.replaced.find(it);
        if(rit != tmp_state.replaced.end())
          for(auto&& x : rit->second)
            if(x == it->get()){
              return passed(*it++);
            }
      }
      static auto pull_out_hash = [](auto&& list){
        for(auto _it = list.begin(); _it != list.end();++_it)
          if(_it->type() == token_type::punctuator_hash || _it->type() == token_type::punctuator_hashhash)
            _it->type() = token_type::punctuator;
      };
      auto copy_insert = [&](auto&& list, auto&& it_, auto beg_, auto end_){
        std::list<token_t> copy(beg_, end_);
        {
            if(!recur.empty()){
              for(auto itt = copy.begin(); itt != copy.end(); ++itt)
                tmp_state.replaced[itt] = recur;
            }
        }
        pull_out_hash(copy);
        if(beg_ != end_){
          auto replaced = (list|replacer(it_, std::next(it_), std::move(copy)));
          it_ = replaced.end();
        }
        else{
          auto replaced = (list|replacer(it_, std::next(it_), token_t{{"", token_type::empty}, beg_->annotation()}));
          it_ = replaced.end();
        }
      };
      auto copy_eval_insert = [&](auto&& cei, auto&& ls, auto&& it_, auto beg_, auto end_, auto index, auto& tmp_state){
        std::list<token_t> list(beg_, end_);
        if(list.size() == 0){
          copy_insert(ls, it_, beg_, end_);
          return;
        }
        pull_out_hash(list);
        auto backup = list;
        pp_state ps{list, tmp_state.replaced};
        for(auto itr = beg_, list_it = list.begin(); itr != end_; ++list_it, ++itr){
          const auto finded = tmp_state.replaced.find(itr);
          if(finded != tmp_state.replaced.end())
            ps.replaced[list_it] = std::move(finded->second);
        }
        {
          auto p = ps.replaced.find(it_);
          if(p != ps.replaced.end()){
            if(!recur.empty()){
              for(auto itt = list.begin(); itt != list.end(); ++itt){
                if(std::any_of(ps.replaced[itt].begin(), ps.replaced[itt].end(), [&](auto&& t){return t == itt->get();}))
                  (ps.replaced[itt] = recur).emplace_back(itt->get());
                else
                  ps.replaced[itt] = recur;
                ps.replaced[itt] = recur;
              }
            }
          }
        }
        auto list_it = list.begin();
        auto func_yield = [&](auto it_, auto id, auto&& ls, auto&& ret){
          if(it_ != ls.begin()){
            ret.emplace(ret.begin(), ls.begin(), it_);
          }
          ++it_;
          ++id;
          auto b = it_;
          while(it_ != ls.end()){
            const auto ai = f->second.arg_index[id];
            if(ai != 0){
              if(b != it_)
                ret.emplace_back(b, it_);
              b = std::next(it_);
            }
            if(ai > 0)
              ret.emplace_back(args[ai-1]);
            else if(ai < 0)
              ret.emplace_back(args[-ai-1].begin(), args.back().end());
            ++it_;
            ++id;
          }
          if(b != it_)
            ret.emplace_back(b, it_);
        };
        while(self.template operator()<InArithmeticEvaluation>(self, passed_identity, state, ps, list_it, list.end(), std::function<void(const phase4_t&, pp_state&, std::list<token_t>::const_iterator, std::vector<output_range<std::list<token_t>::const_iterator>>)>{[&](const phase4_t& state_, pp_state& tmp_state_, std::list<token_t>::const_iterator itr, std::vector<output_range<std::list<token_t>::const_iterator>> list_){
              if(list_it != list.begin()){
                list_.emplace(list_.begin(), list.begin(), list_it);
              }
              func_yield(it_, index, ls, list_);
              list_.emplace_back(arg_it, end);
              yield(state_, tmp_state_, itr, std::move(list_));}}));
        {
          if(!tokens_equal(list, backup)){
              std::vector<std::string> recur_;
              bool first = true;
              for(auto it_ = ls.begin(); it_ != ls.end() && (first || !recur_.empty()); ++it_){
                auto rit = ps.replaced.find(it_);
                if(rit != ps.replaced.end()){
                  if(first)
                    recur_ = rit->second, first = false;
                  else
                    recur_ = vector_product(std::move(recur_), rit->second);
                }
                else
                  recur_.clear(), first = false;
              }
            cei(cei, ls, it_, list.begin(), list.end(), index, ps);
            std::swap(tmp_state.replaced, ps.replaced);
            return;
          }
          std::swap(tmp_state.replaced, ps.replaced);
          tmp_state.replaced.erase(it_);
          auto replaced = (ls|replacer(it_, std::next(it_), std::move(list)));
          it_ = replaced.end();
          return;
        }
      };
      std::list<token_t> copy(f->second.dst.begin(), f->second.dst.end());
      for(auto&& x : copy)
        x.annotation() = it->annotation();
      {
        auto p = tmp_state.replaced.find(it);
        if(p != tmp_state.replaced.end()){
          if(!recur.empty()){
            for(auto itt = copy.begin(); itt != copy.end(); ++itt){
              for(auto&& x : recur)
                if(x == itt->get())
                  tmp_state.replaced[itt].emplace_back(x);
              tmp_state.replaced[itt] = recur;//p->second;
            }
          }
        }
      }
      std::size_t index = 0;
      auto func_yield = [&](auto it_, std::size_t id){
        std::vector<output_range<std::list<token_t>::const_iterator>> ret;
        if(it_ != copy.begin())
          ret.emplace_back(copy.begin(), it_);
        auto b = it_;
        while(it_ != copy.end() && id < f->second.arg_index.size()){
          const auto ai = f->second.arg_index[id];
          if(ai != 0){
            if(b != it_)
              ret.emplace_back(b, it_);
            b = std::next(it_);
          }
          if(ai > 0)
            ret.emplace_back(args[ai-1]);
          else if(ai < 0)
            ret.emplace_back(args[-ai-1].begin(), args.back().end());
          ++it_;
          ++id;
        }
        if(b != it_)
          ret.emplace_back(b, it_),
          b = std::next(it_);
        ret.emplace_back(arg_it, end);
        yield(state, tmp_state, it_, ret);
      };
      func_yield(copy.begin(), index);
      for(auto it_ = copy.begin(); it_ != copy.end();){
        if(it_->type() == token_type::punctuator_hash){
          static auto search = [](auto it, auto sentinel){
            try{
              return search_(std::move(it), std::move(sentinel), [](auto&& it){++it;});
            }catch(std::runtime_error&){
              throw std::runtime_error("last # : operator # must receive argument");
            }
          };
          auto next = search(it_, copy.end());
          const auto next_i = std::distance(it_, next);
          const auto next_ai = f->second.arg_index[index+next_i];
          if(next_ai == 0)
            throw std::runtime_error("# receive invalid(not argument) parameter");
          auto replaced = (copy|replacer(it_, std::next(next), token_t{{'"' + 
                  (next_ai < 0 ? stringizer(args[-next_ai-1].begin(), args.back().end())
                               : stringizer(args[ next_ai-1])
                  ) + '"', token_type::string_literal}, it_->annotation()}));
          it_ = replaced.end();
          index += next_i+1;
          func_yield(it_, index);
          continue;
        }
        if(it_->type() == token_type::punctuator_hashhash){
          static auto search = [](auto it, auto sentinel){
            try{
              return search_(std::move(it), std::move(sentinel), [](auto&& it){++it;});
            }catch(std::runtime_error&){
              throw std::runtime_error("operator ## can't receive parameter(search_parameter reach edge)");
            }
          };
          auto next = search(it_, copy.end());
          const auto next_next = std::next(next);
          const auto next_i = index + std::distance(it_, next);
          const auto next_ai = f->second.arg_index[next_i];
          if(next_ai < 0)
            copy_insert(copy, next, args[-next_ai-1].begin(), args.back().end());
          else if(next_ai > 0)
            copy_insert(copy, next, args[ next_ai-1].begin(), args[next_ai-1].end());
          apply_cat(it_, tmp_state);
          it_ = next_next;
          index = next_i+1;
          func_yield(it_, index);
          continue;
        }
        const auto ai = f->second.arg_index[index];
        static auto search = [](auto it, auto sentinel){
          try{
            return search_(std::move(it), std::move(sentinel), [](auto&& it){++it;});
          }catch(std::runtime_error&){
            return sentinel;
          }
        };
        const auto next = search(it_, copy.end());
        if(ai == 0){
          ++it_;
          ++index;
          continue;
        }
        else if(next != copy.end() && next->type() == token_type::punctuator_hashhash)
          if(ai < 0)
            copy_insert(copy, it_, args[-ai-1].begin(), args.back().end());
          else
            copy_insert(copy, it_, args[ ai-1].begin(), args[ai-1].end());
        else
          if(ai < 0)
            copy_eval_insert(copy_eval_insert, copy, it_, args[-ai-1].begin(), args.back().end(), index, tmp_state);
          else
            copy_eval_insert(copy_eval_insert, copy, it_, args[ ai-1].begin(), args[ai-1].end(), index, tmp_state);
        ++index;
      }
      for(auto it_ = copy.begin(), end_ = copy.end(); it_ != end_; ++it_){
        if(tmp_state.replaced[it_].empty())
          tmp_state.replaced[it_] = recur;
        else
          if(std::any_of(tmp_state.replaced[it_].begin(), tmp_state.replaced[it_].end(), [&](auto&& t){return t == it_->get();})){
            tmp_state.replaced[it_].emplace_back(it_->get());
          }
        tmp_state.replaced[it_].emplace_back(it->get());
      }
      for(auto i = it; i != arg_it; ++i)
        tmp_state.replaced.erase(i);
      auto replaced = (tmp_state.list|replacer(it, arg_it, std::move(copy)));
      it = replaced.begin();
      return true;
    }
  }static constexpr eval_macro = {};
  template<typename T>
  std::optional<std::filesystem::path> find_include_path(T&& tmp, const std::filesystem::path current_path)const{
    std::filesystem::path include_file;
    bool is_angle = false;
    {
      if(tmp.begin()->type() == token_type::header_name){
        auto&& str = tmp.begin()->get();
        include_file = str.substr(1, str.size()-2);
        is_angle = str[0] == '<';
      }
      else{
        std::string str;
        for(auto&& x : tmp)
          if(str.empty() && x.type() == token_type::white_space)
            continue;
          else
            str += x.get();
        const auto parser =
          ( _('<') >> +(veiler::pegasus::read - _('>'))[veiler::pegasus::semantic_actions::value] >> _('>') )[veiler::pegasus::semantic_actions::change_container<std::string>][([&](auto&& v, [[maybe_unused]] auto&&... unused){include_file = std::move(v); is_angle = true; return veiler::pegasus::unit{};})]
          | ( _('"') >> +(veiler::pegasus::read - _('"'))[veiler::pegasus::semantic_actions::value] >> _('"') )[veiler::pegasus::semantic_actions::change_container<std::string>][([&](auto&& v, [[maybe_unused]] auto&&... unused){include_file = std::move(v); is_angle = false;return veiler::pegasus::unit{};})];
        auto result = parser(str.begin(), str.end());
        if(!result)
          return std::nullopt;
      }
    }
    auto f = [&](const std::filesystem::path& path){
      if(!std::filesystem::exists(path) || std::filesystem::is_directory(path))
        return false;
      return true;
    };
    if(!is_angle && std::filesystem::exists(current_path/include_file))
      return std::filesystem::canonical(current_path/include_file);
    for(auto&& x : include_dir)
      if(f(x/include_file))
        return std::filesystem::canonical(x/include_file);
    for(auto&& x : system_include_dir)
      if(f(x/include_file))
        return std::filesystem::canonical(x/include_file);
    return std::nullopt;
  }
  struct arithmetic_expression : veiler::pegasus::parsers<arithmetic_expression>{
    static auto entrypoint(){
      return instance().conditional_expression.with_skipper(*instance()._);
    }
   private:
    static constexpr auto omit = veiler::pegasus::semantic_actions::omit;
    template<typename T>
    static constexpr auto lit(T&& t){return veiler::pegasus::lit(std::forward<T>(t));}
#define SIMPLE_RULE(name, operator_name, operator, next_rule) \
    RULE(name, std::intmax_t, veiler::pegasus::transient(\
         ( rules.next_rule\
        >> *( lit(token_type::operator_name)[omit]\
           >> rules.next_rule\
            )\
         )[([](auto&& v, [[maybe_unused]] auto&&... unused){\
            return std::accumulate(v.begin()+1, v.end(), *v.begin(), [](auto&& lhs, auto&& rhs){return lhs operator rhs;});\
          })]\
        ))
#define TWO_OPERATOR_RULE(name, operator_name1, operator1, operator_name2, operator2, next_rule) \
    RULE(name, std::intmax_t, veiler::pegasus::transient(\
         ( rules.next_rule\
        >> *( ( lit(token_type::operator_name1)[omit][([]([[maybe_unused]]auto&&... unused){return true;})]\
              | lit(token_type::operator_name2)[omit][([]([[maybe_unused]]auto&&... unused){return false;})]\
              )\
           >> rules.next_rule\
            )\
         )[([](auto&& v, [[maybe_unused]] auto&&... unused){\
            auto&& [first, exps] = v;\
            for(auto&& [op, val] : exps)\
              if(op)\
                first = first operator1 val;\
              else\
                first = first operator2 val;\
            return first;\
          })]\
        ))
    RULE(conditional_expression, std::intmax_t, veiler::pegasus::transient(
         ( rules.logical_or
        >> *( lit(token_type::punctuator_question)[omit]
           >> rules.logical_or
           >> lit(token_type::punctuator_colon)[omit]
           >> rules.logical_or
            )
         )[([](auto&& v, [[maybe_unused]] auto&&... unused){
            auto&& [first, tf] = v;
            for(auto&& x : tf)
              if(first == 0)
                first = x[1];
              else
                return x[0];
            return first;
          })]
        ))
    SIMPLE_RULE(logical_or , punctuator_logical_or , ||, logical_and)
    SIMPLE_RULE(logical_and, punctuator_logical_and, &&, bitwise_or )
    SIMPLE_RULE(bitwise_or , punctuator_bitwise_or ,  |, bitwise_xor)
    SIMPLE_RULE(bitwise_xor, punctuator_bitwise_xor,  ^, bitwise_and)
    SIMPLE_RULE(bitwise_and, punctuator_ampersand  ,  &, equal)
    TWO_OPERATOR_RULE(equal, punctuator_equalequal, ==,
                             punctuator_not_equal,  !=, compare)
    AUTO_RULE(compare, veiler::pegasus::transient(
         ( rules.shift
        >> *( ( lit(token_type::punctuator_less)
              | lit(token_type::punctuator_less_equal)
              | lit(token_type::punctuator_greater)
              | lit(token_type::punctuator_greater_equal)
              )[([](auto&& v, [[maybe_unused]]auto&&... unused){return v->type();})]
           >> rules.shift
            )
         )[([](auto&& v, [[maybe_unused]] auto&&... unused){
            auto&& [first, cmps] = v;
            for(auto&& [op, val] : cmps)
              switch(op){
              case token_type::punctuator_less:          first = first <  val; break;
              case token_type::punctuator_less_equal:    first = first <= val; break;
              case token_type::punctuator_greater:       first = first >  val; break;
              case token_type::punctuator_greater_equal: first = first >= val; break;
              default:                                                         break;
              }
            return first;
          })]
        ))
    TWO_OPERATOR_RULE(shift,  punctuator_left_shift,  <<,
                              punctuator_right_shift, >>, addsub)
    TWO_OPERATOR_RULE(addsub, punctuator_plus,         +,
                              punctuator_minus,        -, muldiv)
    AUTO_RULE(muldiv, veiler::pegasus::transient(
         ( rules.unary
        >> *( ( lit(token_type::punctuator_asterisk)
              | lit(token_type::punctuator_division)
              | lit(token_type::punctuator_modulo)
              )[([](auto&& v, [[maybe_unused]]auto&&... unused){return v->type();})]
           >> rules.unary
            )
         )[([](auto&& v, [[maybe_unused]] auto&&... unused){
            auto&& [first, muls] = v;
            for(auto&& [op, val] : muls)
              switch(op){
              case token_type::punctuator_asterisk: first = first * val; break;
              case token_type::punctuator_division: first = first / val; break;
              case token_type::punctuator_modulo:   first = first % val; break;
              default:                                                   break;
              }
            return first;
          })]
        ))
    AUTO_RULE(unary, veiler::pegasus::transient(
         ( *( ( lit(token_type::punctuator_plus)
              | lit(token_type::punctuator_minus)
              | lit(token_type::punctuator_logical_not)
              | lit(token_type::punctuator_bitwise_not)
              )[([](auto&& v, [[maybe_unused]]auto&&... unused){return v->type();})]
            )
        >> rules.primary
         )[([](auto&& v, [[maybe_unused]] auto&&... unused){
            auto&& [ops, val] = v;
            for(auto&& op : ops | boost::adaptors::reversed)
              switch(op){
              case token_type::punctuator_plus:                    break;
              case token_type::punctuator_minus:       val = -val; break;
              case token_type::punctuator_logical_not: val = !val; break;
              case token_type::punctuator_bitwise_not: val = ~val; break;
              default:                                             break;
              }
            return val;
          })]
        ))
    AUTO_RULE(primary, veiler::pegasus::transient(
        ( lit(token_type::pp_number)[([](auto&& v, [[maybe_unused]] auto&&... unused)->veiler::expected<std::intmax_t, veiler::pegasus::parse_error<std::decay_t<decltype(v)>>>{
            std::size_t index;
            auto vstr = v->get();
            vstr.erase(std::remove_if(vstr.begin(), vstr.end(), [](char c){return c == '\'';}), vstr.end());
            auto value = std::stoull(vstr, &index, 0);
            {
              bool is_unsigned = false;
              bool is_long = false;
              bool is_long_long = false;
              for(;index < vstr.size(); ++index){
                switch(vstr[index]){
                case 'u':case'U':
                  if(std::exchange(is_unsigned, true))
                    return veiler::make_unexpected(veiler::pegasus::error_type::semantic_check_failed{"duplicate u suffix"});
                  break;
                case 'l':case 'L':
                  if(index+1 < vstr.size() && vstr[index] == vstr[index+1]){
                    ++index;
                    if(is_long)
                      return veiler::make_unexpected(veiler::pegasus::error_type::semantic_check_failed{"exists l suffix and ll suffix together"});
                    if(std::exchange(is_long_long, true))
                      return veiler::make_unexpected(veiler::pegasus::error_type::semantic_check_failed{"duplicate ll suffix"});
                  }
                  else{
                    if(is_long_long)
                      return veiler::make_unexpected(veiler::pegasus::error_type::semantic_check_failed{"exists l suffix and ll suffix together"});
                   if(std::exchange(is_long, true))
                      return veiler::make_unexpected(veiler::pegasus::error_type::semantic_check_failed{"duplicate l suffix"});
                  }
                  break;
                default:
                  return veiler::make_unexpected(veiler::pegasus::error_type::semantic_check_failed{"invalid suffix"});
                }
              }
            }
            return static_cast<std::intmax_t>(value);
          })]
        | lit(token_type::character_literal)[([](auto&& v, [[maybe_unused]] auto&&... unused)->veiler::expected<std::intmax_t, veiler::pegasus::parse_error<std::decay_t<decltype(v)>>>{
            return static_cast<std::intmax_t>(0);
          })]
        | lit(token_type::punctuator_left_parenthesis)[omit]
       >> rules.conditional_expression
       >> lit(token_type::punctuator_right_parenthesis)[omit]
        | lit(token_type::identifier_true)[omit][([]([[maybe_unused]]auto&&... unused){return static_cast<std::intmax_t>(true);})]
        | lit(token_type::identifier_false)[omit][([]([[maybe_unused]]auto&&... unused){return static_cast<std::intmax_t>(false);})]
        | ( lit(token_type::identifier_defined)[omit]
         >> ( ( lit(token_type::punctuator_left_parenthesis)[omit]
             >> rules.identifier
             >> lit(token_type::punctuator_right_parenthesis)[omit]
              )
              | rules.identifier
            )
          )[([](auto&& v, auto&&, auto&& s, [[maybe_unused]] auto&&... unused)->std::intmax_t{
            return v->type() == token_type::identifier_has_include || s.objects.find(v->get()) != s.objects.end() || s.functions.find(v->get()) != s.functions.end() ? 1 : 0;
          })]
        | ( lit(token_type::identifier_has_include)[omit]
         >> lit(token_type::punctuator_left_parenthesis)[omit]
         >> ( lit(token_type::header_name)
            | lit(token_type::string_literal)
            | veiler::pegasus::lexeme[lit(token_type::punctuator_less) >> *(veiler::pegasus::read - lit(token_type::punctuator_greater)) >> lit(token_type::punctuator_greater)]
            )[veiler::pegasus::semantic_actions::location]
         >> lit(token_type::punctuator_right_parenthesis)[omit]
          )[([](auto&& v, auto&&, auto&& s, auto&& c, [[maybe_unused]] auto&&... unused)->std::intmax_t{
            return s.find_include_path(v, c) ? 1 : 0;
          })]
        | rules.identifier[omit][([]([[maybe_unused]]auto&&... unused){return static_cast<std::intmax_t>(0);})]
        )
        ))
    INLINE_RULE(identifier, veiler::pegasus::filter([](auto&& v, [[maybe_unused]] auto&&... unused){return is_identifier((v++)->type());}))
    INLINE_RULE(_, veiler::pegasus::filter([](auto&& v, [[maybe_unused]] auto&&... unused){return (v++)->type() == token_type::white_space;}))
#undef TWO_OPERATOR_RULE
#undef SIMPLE_RULE
  };
  struct preprocessing_file : veiler::pegasus::parsers<preprocessing_file>{
    static auto entrypoint(){
      return instance().group.with_skipper(*instance()._);
    }
    struct if_section_t;
    struct node{
      std::vector<std::variant<if_section_t, veiler::pegasus::iterator_range<std::list<phase3_t::value_type>::const_iterator>>> data;
    };
    struct if_section_t{
      std::vector<std::tuple<veiler::pegasus::iterator_range<std::list<phase3_t::value_type>::const_iterator>, node>> data;
    }; 
   private:
    static constexpr auto location = veiler::pegasus::semantic_actions::location;
    static constexpr auto omit = veiler::pegasus::semantic_actions::omit;
    static constexpr auto read = veiler::pegasus::read;
    static constexpr auto value = veiler::pegasus::semantic_actions::value;
    template<typename T>
    static constexpr auto lit(T&& t){return veiler::pegasus::lit(std::forward<T>(t));}
    template<typename T, typename U>
    static constexpr auto range(T&& t, U&& u){return veiler::pegasus::range(std::forward<T>(t), std::forward<U>(u));}
    RULE(group, node, veiler::pegasus::transient((*rules.group_part)[([](auto&& v, [[maybe_unused]] auto&&... unused){return node{std::move(v)};})]))
    AUTO_RULE(group_part, veiler::pegasus::transient(
        ( rules.if_section
        | rules.other_part
        )
        ))
    RULE(if_section, if_section_t, veiler::pegasus::transient(
        (  rules.if_group
        >> *( omit[ lit(token_type::eol)
                 >> lit(token_type::punctuator_hash)
                  ]
           >> ( lit(token_type::identifier_elif)
             >> rules.get_line
              )[location]
           >> rules.group
            )
        >> -( omit[ lit(token_type::eol)
                 >> lit(token_type::punctuator_hash)
                  ]
           >> ( lit(token_type::identifier_else)
             >> &lit(token_type::eol)[omit]
              )[location]
           >> rules.group
            )
        >> omit[ lit(token_type::eol)
              >> lit(token_type::punctuator_hash)
              >> lit(token_type::identifier_endif)
              >> &lit(token_type::eol)
               ]
        )[([](auto&& v, [[maybe_unused]] auto&&... unused){
           auto&& [xs, else_part] = v;
           if(else_part)
             xs.emplace_back(std::move(*else_part));
           return if_section_t{std::move(xs)};
         })]
        ))
    AUTO_RULE(if_group, veiler::pegasus::transient(
        omit[ lit(token_type::eol)
           >> lit(token_type::punctuator_hash)
            ]
     >> ( ( lit(token_type::identifier_if)
         >> rules.get_line
         >> &lit(token_type::eol)
          )
          |
          ( ( lit(token_type::identifier_ifdef)
            | lit(token_type::identifier_ifndef)
            )
         >> rules.identifier
         >> &lit(token_type::eol)
          )
        )[location]
     >> rules.group
        ))
    AUTO_RULE(other_part, veiler::pegasus::transient(
        ( lit(token_type::eol)
       >>  ( rules.get_line
           - ( lit(token_type::punctuator_hash)
            >> ( lit(token_type::identifier_if)
               | lit(token_type::identifier_ifdef)
               | lit(token_type::identifier_ifndef)
               | lit(token_type::identifier_elif)
               | lit(token_type::identifier_else)
               | lit(token_type::identifier_endif)
               )
             )
           ) % lit(token_type::eol)
        )[location]
        ))
    INLINE_RULE(get_line, *(veiler::pegasus::read - veiler::pegasus::lit(token_type::eol)) >> &veiler::pegasus::lit(token_type::eol))
    INLINE_RULE(identifier, veiler::pegasus::filter([](auto&& v, [[maybe_unused]] auto&&... unused){return is_identifier((v++)->type());}))
    INLINE_RULE(_, veiler::pegasus::filter([](auto&& v, [[maybe_unused]] auto&&... unused){return (v++)->type() == token_type::white_space;}))
  };
  struct override_annotate{
    std::string filename;
    std::size_t base_line;
    std::size_t org_line;
  };
 public:
  template<bool InArithmeticEvaluation = false, typename T>
  std::list<token_t> eval(std::list<phase3_t::value_type>& ls, T&& r, override_annotate& override_annotation, const std::filesystem::path& current_path, bool step_flag = false, std::ostream& os = std::cout){
    static auto pp_directive_line = 
         _(token_type::eol) >> *_(token_type::white_space)
      >> _(token_type::punctuator_hash) >> *_(token_type::white_space)
      ;
    static auto next_pp_line = [](auto it, auto&& end){
       for(;it != end; ++it)
         if((&pp_directive_line)(it, end))
           break;
       return it;
    };
    struct include_data : veiler::pegasus::iterator_range<std::list<token_t>::const_iterator>{};
    static auto rule_include = 
         _(token_type::identifier_include) >> *_(token_type::white_space)
      >> (+(veiler::pegasus::read - _(token_type::eol)))[veiler::pegasus::semantic_actions::omit][([](auto&&, auto&& loc, [[maybe_unused]] auto&&... unused){return include_data{loc};})];
    using define_data = std::variant<std::tuple<std::list<token_t>::const_iterator, func_t>, std::tuple<std::list<token_t>::const_iterator, output_range<std::list<token_t>::const_iterator>>>;
    static auto identifier = veiler::pegasus::filter([](auto&& v, [[maybe_unused]] auto&&... unused){return is_identifier((v++)->type());});
    static auto rule_define = (
         _(token_type::identifier_define) >> *_(token_type::white_space)
      >> identifier
      >> -( _(token_type::punctuator_left_parenthesis) >> *_(token_type::white_space)
         >> (  ( identifier >> *_(token_type::white_space) )
               % (  _(token_type::punctuator_comma) >> *_(token_type::white_space) )
            >> ( -(  _(token_type::punctuator_comma) >> *_(token_type::white_space)
                  >> _(token_type::punctuator_ellipsis) >> *_(token_type::white_space)
                  )
               )[([](auto&& v, [[maybe_unused]] auto&&... unused){return static_cast<bool>(v);})]
            |  ( -( _(token_type::punctuator_ellipsis) >> *_(token_type::white_space) )
               )[([](auto&& v, auto&& loc, [[maybe_unused]] auto&&... unused){return std::make_tuple(std::vector<std::decay_t<decltype(loc.begin())>>{}, static_cast<bool>(v));})]
            )
            //(), (ident, ident) or (...) or (ident, ...)
         >> _(token_type::punctuator_right_parenthesis)
          )
      >> *_(token_type::white_space)
      >> (*(veiler::pegasus::read - _(token_type::eol)))[veiler::pegasus::semantic_actions::location]
      )[([](auto&& t, auto&& loc, [[maybe_unused]] auto&&... args)->veiler::expected<define_data, veiler::pegasus::parse_error<std::decay_t<decltype(loc.begin())>>>{
            auto&& [name, function_info, destination] = t;
            if(name->type() == token_type::identifier_defined)
              return veiler::make_unexpected(veiler::pegasus::error_type::semantic_check_failed{});
            output_range<std::list<token_t>::const_iterator> dst{destination.begin(), destination.end()};
            if(function_info){
              auto&& [args, is_variadic] = *function_info;
              std::vector<int> arg_index;
              arg_index.reserve(std::distance(dst.begin(), dst.end()));
              {
                std::size_t t_i = 0;
                for(auto&& t : dst){
                  if(is_variadic && t.get() == "__VA_ARGS__"){
                    arg_index.push_back(-static_cast<int>(args.size())-1);
                    ++t_i;
                    continue;
                  }
                  for(auto&& x : args | boost::adaptors::indexed())
                    if(t.get() == x.value()->get()){
                      arg_index.push_back(x.index()+1);
                      break;
                    }
                  if(t_i++ == arg_index.size())
                    arg_index.push_back(0);
                }
              }
              return define_data{std::make_tuple(std::move(name), func_t{is_variadic ? -static_cast<int>(args.size())-1 : static_cast<int>(args.size()), std::move(arg_index), dst})};
            }
            else
              return define_data{std::make_tuple(std::move(name), dst)};
          })];
    using undef_data = std::list<token_t>::const_iterator;
    static auto rule_undef = 
         _(token_type::identifier_undef) >> *_(token_type::white_space)
      >> _(token_type::identifier)[([](auto&&, auto&& loc, [[maybe_unused]] auto&&... unused)->undef_data{return loc.begin();})];
    struct pragma_step_data : veiler::pegasus::iterator_range<std::list<token_t>::const_iterator>{};
    static const auto rule_pragma_step =
         _(token_type::identifier_pragma) >> *_(token_type::white_space)
      >> _(std::string_view{"step"}) >> *_(token_type::white_space)
      >> (*(veiler::pegasus::read - _(token_type::eol)))[([](auto&&, auto&& loc, [[maybe_unused]] auto&&... unused){return pragma_step_data{loc};})];
    static auto rule_pragma = 
         _(token_type::identifier_pragma) >> *_(token_type::white_space)
      >> *(veiler::pegasus::read - _(token_type::eol));
    using error_data = std::tuple<std::string_view, std::size_t, std::size_t, veiler::pegasus::iterator_range<std::list<token_t>::const_iterator>>;
    static auto rule_error = 
         veiler::pegasus::lit(token_type::identifier_error)[([](auto&& v, [[maybe_unused]] auto&&... unused){return std::make_tuple(v->filename(), v->line(), v->column());})] >> *_(token_type::white_space)
      >> (*(veiler::pegasus::read - _(token_type::eol)))[veiler::pegasus::semantic_actions::location];
    struct line_data : veiler::pegasus::iterator_range<std::list<token_t>::const_iterator>{};
    static auto rule_line = 
         _(token_type::identifier_line) >> *_(token_type::white_space)
      >> (+(veiler::pegasus::read - _(token_type::eol)))[veiler::pegasus::semantic_actions::omit][([](auto&&, auto&& loc, [[maybe_unused]] auto&&... unused){return line_data{loc};})];
    static auto pp_directive = 
       pp_directive_line[veiler::pegasus::semantic_actions::omit]
    >> ( rule_include
       | rule_define
       | rule_undef
       | rule_pragma_step
       | rule_pragma[veiler::pegasus::semantic_actions::omit]
       | rule_error
       | rule_line
       | veiler::pegasus::eps[veiler::pegasus::semantic_actions::omit]
       );
    pp_state preprocessing_state{ls, {}};
    std::list<token_t> result;
    auto passed = [&](auto&& t){result.push_back(t);return true;};
    for(auto it = r.begin(); it != r.end();)
      if((&pp_directive_line)(it, r.end())){
        if(auto ret = pp_directive(it, r.end())){
          if(*ret){
            struct{
              void operator()(const include_data& i)const{
                std::list<token_t> tmp;
                {
                  auto it = i.begin();
                  while(it != i.end())
                    if(!eval_macro(eval_macro, [&tmp](auto&& t){tmp.push_back(t);return true;}, *s_, *pps_, it, i.end(), [](const phase4_t&, pp_state&, std::list<token_t>::const_iterator, const std::vector<output_range<std::list<token_t>::const_iterator>>&){}))
                    {return;}
                }
                if(tmp.empty())
                  return;
                auto path = s_->find_include_path(tmp, current_path);
                if(path){
                  auto canonicaled = path->string();
                  if(s_->files.find(canonicaled) == s_->files.end()){
                    std::ifstream ifs{*path};
                    std::istreambuf_iterator<char> it{ifs};
                    static constexpr std::istreambuf_iterator<char> end{};
                    std::string tmp(it, end);
                    static constexpr phase1_t phase1;
                    static constexpr phase2_t phase2;
                    static constexpr phase3_t phase3;
                    auto range = tmp | annotation{canonicaled} | phase1 | phase2 | phase3;
                    std::list<token_t> tokens(range.begin(), range.end());
                    auto copied = canonicaled;
                    s_->files[std::move(canonicaled)] = std::move(tokens);
                    canonicaled = std::move(copied);
                  }
                  res_->splice(res_->end(), (*s_)(s_->files[canonicaled], path->parent_path()));
                  return;
                }
                return;
              }
              void operator()(define_data&& d)const{
                struct{
                  void operator()(std::tuple<std::list<token_t>::const_iterator, func_t>&& t)const{
                    s_->functions.emplace(std::get<0>(std::move(t))->get(), std::get<1>(std::move(t)));
                  }
                  void operator()(std::tuple<std::list<token_t>::const_iterator, output_range<std::list<token_t>::const_iterator>>&& t)const{
                    s_->objects.emplace(std::get<0>(std::move(t))->get(), std::get<1>(std::move(t)));
                  }
                  phase4_t* s_;
                }v{s_};
                std::visit(v, std::move(d));
              }
              void operator()(const undef_data& u)const{
                auto& s = *s_;
                auto&& x = u->get();
                auto o = s.objects.find(x);
                if(o != s.objects.end()){
                  s.objects.erase(o);
                  return;
                }
                auto f = s.functions.find(x);
                if(f != s.functions.end())
                  s.functions.erase(f);
              }
              void operator()(const error_data& e)const{
                std::stringstream ss;
                for(auto&& x : std::get<3>(e))
                  ss << x.get();
                throw std::runtime_error(std::string{std::get<0>(e)} + ':' + std::to_string(std::get<1>(e)) + ':' + std::to_string(std::get<2>(e)) + ": error: " + ss.str());
              }
              void operator()(const line_data& l)const{
                std::list<token_t> tmp;
                {
                  auto it = l.begin();
                  while(it != l.end())
                    if(!eval_macro(eval_macro, [&tmp](auto&& t){tmp.push_back(t);return true;}, *s_, *pps_, it, l.end(), [](const phase4_t&, pp_state&, std::list<token_t>::const_iterator, const std::vector<output_range<std::list<token_t>::const_iterator>>&){}))
                      {return;}
                }
                if(tmp.empty())
                  return;
                constexpr auto parser = (
                   veiler::pegasus::lit(token_type::pp_number)[([](auto&& v, [[maybe_unused]] auto&&... unused)->veiler::expected<std::size_t, veiler::pegasus::parse_error<std::list<token_t>::const_iterator>>{
                     std::size_t idx;
                     const auto ret = std::stoull(v->get(), &idx, 10);
                     if(idx != v->get().size())
                       return veiler::make_unexpected<veiler::pegasus::parse_error<std::list<token_t>::const_iterator>>(veiler::pegasus::error_type::semantic_check_failed{"line number is not decimal"});
                     return ret;
                   })]
                >> -veiler::pegasus::lit(token_type::string_literal)[veiler::pegasus::semantic_actions::value]
                ).with_skipper(*_(token_type::white_space));
                auto cit = tmp.cbegin();
                auto result = parser(cit, tmp.cend());
                if(!result || cit != tmp.cend())
                  return;
                auto&& [line_num, filename] = *result;
                if(filename){
                  auto str_lit = string_literal::destringize(*filename);
                  if(str_lit)
                    oa_->filename = str_lit->str;
                  else
                    return;
                }
                oa_->base_line = line_num;
                static auto next_line = [](auto it, auto end){
                  while(it != end)
                    if(it->type() == token_type::eol)
                      return ++it;
                    else
                      ++it;
                  return it;
                };
                const auto nit = next_line(l.end(), end);
                oa_->org_line = nit->line();
                for(auto it = nit; it != end; ++it){
                  const_cast<phase3_t::value_type&>(*it).line(oa_->base_line + it->line() - oa_->org_line);
                  if(!oa_->filename.empty())
                    const_cast<phase3_t::value_type&>(*it).filename(oa_->filename);
                }
              }
              void operator()(const pragma_step_data& p)const{
                res_->splice(res_->end(), 
                    s_->eval(pps_->list, static_cast<const veiler::pegasus::iterator_range<std::list<token_t>::const_iterator>&>(p), *oa_, current_path, true) );
              }
              phase4_t* s_;
              pp_state* pps_;
              override_annotate* oa_;
              decltype(std::declval<T>().end()) end;
              const std::filesystem::path& current_path;
              std::list<token_t>* res_;
            }visitor{this, &preprocessing_state, &override_annotation, ls.end(), current_path, &result};
            std::visit(visitor, std::move(**ret));
          }
        }
        else
          ++it;
      }
      else if(step_flag){
        boost::coroutines2::coroutine<std::vector<output_range<std::list<token_t>::const_iterator>>>::pull_type coroutine{[&](boost::coroutines2::coroutine<std::vector<output_range<std::list<token_t>::const_iterator>>>::push_type& yield){
          preprocessing_state.replaced.clear();
          const auto next_pp = next_pp_line(it, r.end());
          while(it != next_pp)
            if(!eval_macro.template operator()<InArithmeticEvaluation>(eval_macro, passed, *this, preprocessing_state, it, next_pp, [&](const phase4_t&, pp_state&, std::list<token_t>::const_iterator, const std::vector<output_range<std::list<token_t>::const_iterator>>& list){
              yield(list);
            }))
            {std::cout << "eval_macro_failed" << std::endl;return;}
        }};
        const auto next_pp = next_pp_line(it, r.end());
        os << "   ";
        auto pnp = std::prev(next_pp);
        while(result.front().type() == token_type::eol)result.pop_front();
        for(auto&& x : result)
          if(x.type() != token_type::eol)
            os << x;
          else
            os << "\n   ";
        for(auto&& x : output_range<std::list<token_t>::const_iterator>{it, next_pp})
          if(x.type() != token_type::eol || (x.line() == pnp->line() && x.column() == pnp->column()))
            os << x;
          else
            os << "\n   ";
        if(std::prev(next_pp)->type() != token_type::eol)
          os << '\n';
        for(auto&& xss : coroutine){
          os << "-> ";
          while(result.front().type() == token_type::eol)result.pop_front();
          for(auto&& x : result)
            if(x.type() != token_type::eol)
              os << x;
            else
              os << "\n   ";
          auto pxsse = std::prev(xss.back().end());
          for(auto&& xs : xss)
            for(auto&& x : xs){
              if(x.type() != token_type::eol || (x.line() == pxsse->line() && x.column() == pxsse->column()))
                os << x;
              else
                os << "\n   ";
            }
          if(pxsse->type() != token_type::eol)
            os << '\n';
        }
        const auto p6 = phase6(result);
        if(!tokens_equal(result, p6)){
          os << "-> ";
          for(auto&& x : p6)
            if(x.type() != token_type::eol)
              os << x;
            else
              os << "\n   ";
          if(std::prev(p6.end())->type() != token_type::eol)
            os << '\n';
        }
        return {};
      }
      else{
        const auto next_pp = next_pp_line(it, r.end());
        while(it != next_pp)
          if(!eval_macro.template operator()<InArithmeticEvaluation>(eval_macro, passed, *this, preprocessing_state, it, next_pp, [](const phase4_t&, pp_state&, std::list<token_t>::const_iterator, const std::vector<output_range<std::list<token_t>::const_iterator>>&){}))
            {std::cerr << "eval_macro_failed" << std::endl; return decltype(result){};}
      }
    return result;
  }
  auto operator()(std::list<phase3_t::value_type>& ls, const std::filesystem::path& current_path = std::filesystem::current_path()){
    auto if_group = preprocessing_file::entrypoint()(std::as_const(ls));
    if(!if_group){
      std::cerr << "parsing for file structure failed" << std::endl;
      return std::list<phase3_t::value_type>{};
    }
    override_annotate override_annotation = {};
    struct{
      using list = std::list<phase3_t::value_type>;
      using iterator = list::const_iterator;
      using iterator_range = veiler::pegasus::iterator_range<iterator>;
      static iterator next(iterator it){
        do{
          ++it;
        }while(it->type() == token_type::white_space);
        return it;
      }
      list operator()(const preprocessing_file::node& n)const{
        list l;
        for(auto&& x : n.data)
          l.splice(l.end(), std::visit(*this, x));
        return l;
      }
      list operator()(const iterator_range& other_part)const{
        using veiler::pegasus::lit;
        return self->eval(*ls_p, other_part, *oa, *cp, false);
      }
      list operator()(const preprocessing_file::if_section_t& if_section)const{
        for(auto&& [range, node] : if_section.data)
          switch(range.begin()->type()){
          case token_type::identifier_if:
          case token_type::identifier_elif:
            if(arithmetic_expression::entrypoint()(self->eval<true>(*ls_p, iterator_range{next(range.begin()), range.end()}, *oa, *cp), *self, *cp).value() != 0)
              return (*this)(node);
            break;
          case token_type::identifier_ifdef:
          case token_type::identifier_ifndef:{
            const auto ident = next(range.begin());
            assert(is_identifier(ident->type()));
            if(ident->type() == token_type::identifier_has_include){
              if(range.begin()->type() == token_type::identifier_ifdef)
                return (*this)(node);
              else
                return list{};
            }
            const bool function = self->functions.find(ident->get()) != self->functions.end();
            const bool object = self->objects.find(ident->get()) != self->objects.end();
            if((function || object) == (range.begin()->type() == token_type::identifier_ifdef))
              return (*this)(node);
          }break;
          case token_type::identifier_else:
            return (*this)(node);
          default:;
          }
        return list{};
      }
      phase4_t* self;
      std::list<phase3_t::value_type>* ls_p;
      override_annotate* oa;
      const std::filesystem::path* cp;
    }visitor{this, &ls, &override_annotation, &current_path};
    auto ret = visitor(*if_group);
    if(!ret.empty())
      ret.erase(ret.begin()); //first eol
    return ret;
  }
};

#undef INLINE_RULE
#undef AUTO_RULE
#undef RULE

static std::list<phase3_t::value_type> phase6(std::list<phase3_t::value_type> tokens){
  static constexpr auto search = [](std::list<phase3_t::value_type>::const_iterator it, std::list<phase3_t::value_type>::const_iterator end)->std::optional<std::list<phase3_t::value_type>::const_iterator>{
    while(it != end)
      if(is_white_spaces(it->type()))
        ++it;
      else
        return it;
    return std::nullopt;
  };
  std::list<phase3_t::value_type> ret;
  for(auto it = tokens.cbegin(); it != tokens.cend();){
    if(it->type() != token_type::string_literal){
      ret.splice(ret.end(), tokens, it++);
      continue;
    }
    auto str = *string_literal::destringize(*it);
    auto next = search(std::next(it), tokens.cend());
    if(next && (*next)->type() == token_type::string_literal){
      while(true){
        str += *string_literal::destringize(**next);
        auto n = search(++*next, tokens.cend());
        if(n && (*n)->type() == token_type::string_literal)
          next = n;
        else
          break;
      }
      ret.emplace_back(str.stringize(it->annotation()));
    }
    else
      ret.splice(ret.end(), tokens, it++);
    if(next)
      it = *next;
    else
      it = tokens.cend();
  }
  return ret;
}

class split_range{
  std::string_view target;
  std::string delimiters;
 public:
  class iterator{
    std::string_view delimiters;
    std::string_view::const_iterator prev, it, end;
   public:
    iterator() = default;
    iterator(std::string_view delim, const std::string_view::const_iterator& begin, const std::string_view::const_iterator& end):delimiters{delim}, prev{begin}, it{begin}, end{end}{++*this;}
    using value_type = std::string_view;
    iterator& operator++(){
      static constexpr auto f = [](auto& prev, auto it, auto end){
        if(it == end){
          prev = it;
          return false;
        }
        return true;
      };
      while(f(prev, it, end))
        if(delimiters.find(*it) == std::string_view::npos){
          prev = it;
          break;
        }
        else
          ++it;
      while(it != end)
        if(delimiters.find(*it) != std::string_view::npos)
          break;
        else
          ++it;
      return *this;
    }
    value_type operator*()const{return value_type{&*prev, static_cast<std::string_view::size_type>(it-prev)};}
    constexpr bool operator!=(const iterator& rhs){
      auto lt = prev;
      while(lt != end && delimiters.find(*lt) != std::string_view::npos)
        ++lt;
      auto rt = rhs.prev;
      while(rt != end && delimiters.find(*rt) != std::string_view::npos)
        ++rt;
      return lt != rt;
    }
  };
  split_range(std::string_view target, std::string delimiters):target{target}, delimiters{std::move(delimiters)}{}
  iterator begin()const{return iterator{delimiters, target.begin(), target.end()};}
  iterator end()const{return iterator{{}, target.end(), target.end()};}
};

}

#include<linse.hpp>
#include<iostream>

int main(){
  using messer::annotation;
  using namespace std::literals::string_view_literals;
  static constexpr auto white_space = veiler::pegasus::filter([](auto&& v, [[maybe_unused]] auto&&... unused){
    const bool result = v->type() == messer::token_type::white_space;
    if(result)
      ++v;
    return result;
  });
  static constexpr messer::phase1_t phase1;
  static constexpr messer::phase2_t phase2;
  static constexpr messer::phase3_t phase3;
  std::list<std::string> inputed;
  std::list<std::list<decltype(std::string{} | annotation{""} | phase1 | phase2 | phase3)::value_type>> tokens;
  messer::phase4_t preprocessor_data;
  preprocessor_data.system_include_dir.emplace_back("/usr/include");
  linse input;
  input.history.load("./.repl_history");
  auto logical_line = [&input, &preprocessor_data](const char* prompt)->std::optional<std::string>{
    std::string str;
    input.completion_callback = [&](std::string_view data, std::string_view::size_type pos)->linse::completions{
      using veiler::pegasus::lit;
      using messer::token_type;
      linse::completions comp;
      const auto pref = str + std::string{data.substr(0, pos)};
      auto anno_range = pref | annotation{"<complete>"};
      auto range = anno_range| phase1 | phase2 | phase3;
      {
        static constexpr auto include_parser =
           ( veiler::pegasus::semantic_actions::omit[
             lit(token_type::eol)
          >> lit(token_type::punctuator_hash)
          >> lit(token_type::identifier_include)]
          >> &( lit(token_type::punctuator_less)[([]([[maybe_unused]] auto&&... unused){return true;})]
              | lit("\""sv)[([]([[maybe_unused]] auto&&... unused){return true;})]
              )
           ).with_skipper(*white_space);
        static const auto has_include_parser_impl =
           veiler::pegasus::semantic_actions::omit[
           *( veiler::pegasus::read - lit(token_type::identifier_has_include) )
        >> lit(token_type::identifier_has_include)
        >> lit(token_type::punctuator_left_parenthesis)]
        >> &( lit(token_type::punctuator_less)[([]([[maybe_unused]] auto&&... unused){return true;})]
            | lit("\""sv)[([]([[maybe_unused]] auto&&... unused){return false;})]
            );
        static const auto has_include_parser =
           ( veiler::pegasus::semantic_actions::omit[
             lit(token_type::eol)
          >> lit(token_type::punctuator_hash)
          >> lit(token_type::identifier_if)
          >> *( has_include_parser_impl
             >> ( lit(token_type::punctuator_less) >> *(veiler::pegasus::read - lit(token_type::punctuator_greater)) >> lit(token_type::punctuator_greater)
                | lit("\""sv) >> *(veiler::pegasus::read - lit("\""sv)) >> lit("\""sv)
                )
             >> lit(token_type::punctuator_right_parenthesis)
              )]
          >> has_include_parser_impl
           ).with_skipper(*white_space);
        auto it = range.begin();
        const auto include_parser_result = include_parser(it, range.end());
        const auto has_include_parser_result = include_parser_result ? include_parser_result : has_include_parser(it, range.end());
        if(include_parser_result || has_include_parser_result){
          const auto is_angled = include_parser_result ? *include_parser_result : *has_include_parser_result;
          static constexpr auto find_file = [](const std::filesystem::path& include_dir, const std::filesystem::path& path, std::vector<std::string>& bank){
            const auto directory_path = include_dir/path.parent_path();
            if(!std::filesystem::exists(directory_path) || !std::filesystem::is_directory(directory_path))
              return;
            const veiler::pegasus::iterator_range<std::filesystem::directory_iterator> directory{std::filesystem::directory_iterator{directory_path}, std::filesystem::directory_iterator{}};
            const auto filename = path.filename().u8string();
            for(auto&& x : directory){
              const auto filepath = x.path().filename().u8string();
              if(filepath.find(filename) != 0)
                continue;
              bank.emplace_back(std::string_view{filepath}.substr(filename.size()));
              if(x.is_directory())
                bank.back().push_back('/');
            }
          };
          std::filesystem::path path(std::string(messer::get_raw(messer::get_raw(it)).get_inner(), anno_range.end().get_inner()));
          std::vector<std::string> bank;
          if(!is_angled)
            find_file(std::filesystem::path{"."}, path, bank);
          for(auto&& x : preprocessor_data.system_include_dir)
            find_file(x, path, bank);
          std::sort(bank.begin(), bank.end());
          const auto end = std::unique(bank.begin(), bank.end());
          for(auto it = bank.begin(); it != end; ++it)
            comp.add_completion(*it);
          comp.set_prefix(path.filename().u8string());
          return comp;
        }
      }
      static constexpr auto if_parser =
         veiler::pegasus::semantic_actions::omit[
           lit(token_type::eol)
        >> lit(token_type::punctuator_hash)
        >> veiler::pegasus::lexeme[
             lit(token_type::identifier_if)
          >> white_space
           ]
         ].with_skipper(*white_space);
      static constexpr auto ifdef_parser =
         ( veiler::pegasus::semantic_actions::omit[
           lit(token_type::eol)
        >> lit(token_type::punctuator_hash)
        >> ( lit(token_type::identifier_ifdef)
           | lit(token_type::identifier_ifndef)
           )]
        >> veiler::pegasus::filter([](auto&& v, [[maybe_unused]] auto&&... unused){return messer::is_identifier(v->type());})
         ).with_skipper(*white_space);
      static constexpr auto define_fm_parser = (
         veiler::pegasus::semantic_actions::omit[
           lit(token_type::eol)
        >> lit(token_type::punctuator_hash)
        >> lit(token_type::identifier_define)
        >> veiler::pegasus::lexeme[
             veiler::pegasus::filter([](auto&& v, [[maybe_unused]] auto&&... unused){return messer::is_identifier(v->type());}) >> veiler::pegasus::read
          >> lit(token_type::punctuator_left_parenthesis)
           ]
         ]
      >> (veiler::pegasus::filter([](auto&& v, [[maybe_unused]] auto&&... unused){return messer::is_identifier(v->type());})[veiler::pegasus::semantic_actions::omit] >> veiler::pegasus::read) % lit(token_type::punctuator_comma)
      >> ( ( lit(token_type::punctuator_comma)[veiler::pegasus::semantic_actions::omit] >> lit(token_type::punctuator_ellipsis) )[([]([[maybe_unused]] auto&&... unused){return true;})]
         | veiler::pegasus::eps[([]([[maybe_unused]] auto&&... unused){return false;})]
         )
      ).with_skipper(*white_space);
      static constexpr auto undef_parser =
         veiler::pegasus::semantic_actions::omit[
           lit(token_type::eol)
        >> lit(token_type::punctuator_hash)
        >> lit(token_type::identifier_undef)
        >> veiler::pegasus::filter([](auto&& v, [[maybe_unused]] auto&&... unused){return messer::is_identifier(v->type());})
         ].with_skipper(*white_space);
      const auto tokens = std::list<decltype(range)::value_type>(range.begin(), range.end());
      const std::string_view prefix = tokens.back().get();
      comp.set_prefix(prefix);
      std::vector<std::string> bank;
      if(if_parser(tokens)){
        if(prefix.size() <= 13 && "__has_include"sv.compare(0, prefix.size(), prefix) == 0)
          bank.emplace_back("__has_include(");
        else if(prefix.size() <= 7 && "defined"sv.compare(0, prefix.size(), prefix) == 0)
          bank.emplace_back("defined(");
      }else if(auto ifdef_directive = ifdef_parser(tokens)){
        if((*ifdef_directive)->get() != prefix)
          return comp;
        if(prefix.size() <= 13 && "__has_include"sv.compare(0, prefix.size(), prefix) == 0)
          bank.emplace_back("__has_include");
      }else if(auto define_directive = define_fm_parser(tokens)){
        auto&& [identifiers, is_variadic] = *define_directive;
        for(auto&& x : identifiers)
          if(prefix.size() <= x->get().size() && x->get().compare(0, prefix.size(), prefix) == 0)
            bank.emplace_back(x->get());
        if(is_variadic
        && prefix.size() <= 11
        && "__VA_ARGS__"sv.compare(0, prefix.size(), prefix) == 0)
          bank.emplace_back("__VA_ARGS__");
      }else if(auto directive =
        ( veiler::pegasus::semantic_actions::omit[
            lit(token_type::eol)
         >> lit(token_type::punctuator_hash)
          ]
       >> veiler::pegasus::filter([](auto&& v, [[maybe_unused]] auto&&... unused){return messer::is_identifier(v->type());})
        ).with_skipper(*white_space)(tokens))
        if((*directive)->get() == prefix){
          std::string_view directives[] = {
            "define",
            "elif",
            "else",
            "endif",
            "error",
            "if",
            "ifdef",
            "ifndef",
            "include",
            "line",
            "pragma step",
            "undef",
          };
          for(auto&& x : directives)
            if(prefix.size() <= x.size() && x.compare(0, prefix.size(), prefix) == 0)
              comp.add_completion(x.substr(prefix.size()));
          return comp;
        }
      const bool is_undef = undef_parser(tokens).valid();
      auto search_add = [&](auto&& data, char suffix = '\0'){
        for(auto&& x : data)
          if(prefix.size() <= x.first.size() && !x.first.compare(0, prefix.size(), prefix)){
            if(suffix == '\0')
              bank.emplace_back(x.first);
            else
              bank.emplace_back(x.first+suffix);
          }
      };
      search_add(preprocessor_data.objects);
      search_add(preprocessor_data.functions, is_undef ? '\0' : '(');
      for(auto&& x : {
          "true"sv,
          "false"sv,
          "__TIME__"sv,
          "__DATE__"sv,
          "__FILE__"sv,
          "__LINE__"sv,
          "_Pragma("sv,
          "bitand"sv,
          "and_eq"sv,
          "xor_eq"sv,
          "not_eq"sv,
          "bitor"sv,
          "compl"sv,
          "or_eq"sv,
          "and"sv,
          "xor"sv,
          "not"sv,
          "or"sv
        })
        if(prefix.size() <= x.size() && x.compare(0, prefix.size(), prefix) == 0)
          bank.emplace_back(x);
      std::sort(bank.begin(), bank.end());
      const auto end = std::unique(bank.begin(), bank.end());
      for(auto it = bank.begin(); it != end; ++it)
        comp.add_completion(std::string_view{*it}.substr(prefix.size()));
      return comp;
    };
    static constexpr auto check_raw_string = [](auto&& s)->std::optional<std::string>{
      auto range = s | annotation{"<temporary>"} | phase1 | phase2 | phase3;
      auto it = range.begin();
      messer::phase3_t::value_type token{{"", messer::token_type::empty}, {"", 0, 0}};
      static constexpr auto f = [](auto&& raw)->std::optional<std::string>{
        std::string delimiter;
        while(*++raw != '(')
          if(*raw != ' ' && *raw != ')' && *raw != '\\' && *raw != '\t' && *raw != '\v' && *raw != '\f' && *raw != '\n')
            delimiter.push_back(*raw);
          else
            return std::nullopt;
        return delimiter;
      };
      try{
        const auto end = range.end();
        auto prev = messer::get_raw(it);
        while(it != end){
          if(messer::is_identifier(token.type()) && token.get().back() == 'R' && it->get().front() == '"')
            return f(prev);
          prev = messer::get_raw(it);
          token = *it++;
        }
      }catch(...){
        auto raw = messer::get_raw(it);
        if(*raw == '"' && token.type() == messer::token_type::identifier && token.get().back() == 'R')
          return f(raw);
      }
      return std::nullopt;
    };
    while(auto in = input(prompt)){
      str += std::move(*in);
      if(auto raw_string = check_raw_string(str)){
        prompt = "R\"> ";
        str.push_back('\n');
        continue;
      }
      if(str.back() == '\\'){
        prompt = " \\> ";
        str.push_back('\n');
        continue;
      }
      return str;
    }
    return std::nullopt;
  };
  while(auto str = logical_line(">>> ")){
    str->push_back('\n');
    const auto if_directive = [&](auto&& s){
      using veiler::pegasus::lit;
      using messer::token_type;
      static constexpr auto if_rule =
         ( lit(token_type::eol)
        >> lit(token_type::punctuator_hash)
        >> ( lit(token_type::identifier_if)
           | lit(token_type::identifier_ifdef)
           | lit(token_type::identifier_ifndef)
           )
         )[veiler::pegasus::semantic_actions::omit].with_skipper(*white_space);
      return if_rule(s | annotation{"<temporary>"} | phase1 | phase2 | phase3);
    };
    const auto endif_directive = [&](auto&& s){
      using veiler::pegasus::lit;
      using messer::token_type;
      static constexpr auto endif_rule =
         ( lit(token_type::eol)
        >> lit(token_type::punctuator_hash)
        >> lit(token_type::identifier_endif)
         )[veiler::pegasus::semantic_actions::omit].with_skipper(*white_space);
      return endif_rule(s | annotation{"<temporary>"} | phase1 | phase2 | phase3);
    };
    std::size_t if_nest = 0;
    if(if_directive(*str))
      ++if_nest;
    while(if_nest){
      if(auto add = logical_line("if> ")){
        add->push_back('\n');
        if(endif_directive(*add))
          --if_nest;
        else if(if_directive(*add))
          ++if_nest;
        *str += std::move(*add);
      }
      else
        return 0;
    }
    inputed.emplace_back(std::move(*str));
    auto range = inputed.back() | annotation{"<stdin>"} | phase1 | phase2 | phase3;
    tokens.emplace_back(range.begin(), range.end());
    try{
      auto result = phase6(preprocessor_data(tokens.back()));
      if(!result.empty()){
        for(auto&& x : result)
          std::cout << x;
        std::cout << std::endl;
      }
    }catch(std::exception& e){
      std::cerr << e.what() << std::endl;
    }
    for(auto&& x : messer::split_range{inputed.back(), "\n"})
      input.history.add(x);
  }
}


;;;
;;; Test dbd.oracle
;;;

(use dbi)
(use gauche.test)
(use gauche.collection)
(use util.relation)

(test-start "dbd.oracle")
(use dbd.oracle)
(test-module 'dbd.oracle)

(define conn #f)
(define query #f)
(define result #f)

(test* "dbi-connect" '<oracle-connection>
       (let1 c (dbi-connect "dbi:oracle://localhost/XE" :username "ruby" :password "oci8")
	 (set! conn c)
	 (class-name (class-of c))))

(test* "dbi-do create table test" #t
       (begin
         (dbi-do conn "CREATE TABLE test ( \
                         id integer primary key, \
                         name varchar2(50), \
                         position varchar2(50) \
                       )")
         #t))

(test* "dbi-do insert" #t
       (begin
	 (dbi-do conn "INSERT INTO test (id, name, position) \
                         VALUES (10, 'Del Piero', 'FW')")
	 (dbi-do conn "INSERT INTO test (id, name, position) \
                         VALUES (1, 'Buffon', 'GK')")
	 #t))

(test* "dbi-do select" '<oracle-result>
       (let1 r (dbi-do conn "SELECT * from test ORDER BY id")
	 (set! result r)
	 (class-name (class-of r))))

(test* "relation-accessor" '((1 "Buffon" "GK") (10 "Del Piero" "FW"))
       (let1 getter (relation-accessor result)
	     (map (lambda (row)
                (list (getter row "id")
                      (getter row "name")
                      (getter row "position")))
		  result)))

(test* "dbi-prepare insert & execute" '<oracle-query>
       (let1 q (dbi-prepare conn "INSERT INTO test VALUES (?, ?, ?)")
	 (set! query q)
         (dbi-execute query 11 "Nedved" "MF")
         (class-name (class-of q))))

(test* "dbi-open? (query)" #t
       (dbi-open? query))

(test* "dbi-close (query)" #f
       (begin (dbi-close query)
	      (dbi-open? query)))

(test* "dbi-prepare select & execute" '((11 "Nedved" "MF"))
       (let* ([q (dbi-prepare conn "SELECT * FROM test WHERE id=?")]
              [r (dbi-execute q 11)]
              [getter (relation-accessor r)])
	 (map (lambda (row)
                (list (getter row "id")
                      (getter row "name")
                      (getter row "position")))
              r)))

(test* "dbi-prepare select & execute" '()
       (let* ([q (dbi-prepare conn "SELECT * FROM test WHERE id=?")]
              [r (dbi-execute q 30)]
              [getter (relation-accessor r)])
	 (map (lambda (row)
                (list (getter row "id")
                      (getter row "name")
                      (getter row "position")))
              r)))

(test* "dbi-prepare select & execute (error)" '<dbi-parameter-error>
       (let* ([q (dbi-prepare conn "SELECT * FROM test WHERE id=?")]
              [e (guard (e [else e]) (dbi-execute q))])
         (class-name (class-of e))))

(test* "dbi-do drop table test" #t
       (begin (dbi-do conn "DROP TABLE test") #t))

(test* "dbi-open? (connection)" #t
       (dbi-open? conn))

(test* "dbi-close (connection)" #f
       (begin (dbi-close conn)
	      (dbi-open? conn)))

;; epilogue
(test-end)

;;;
;;; dbd.oracle - Oracle driver
;;;
;;;   Copyright (c) 2009 KUBO Takehiro <kubo@jiubao.org>
;;;
;;;   Redistribution and use in source and binary forms, with or without
;;;   modification, are permitted provided that the following conditions
;;;   are met:
;;;
;;;   1. Redistributions of source code must retain the above copyright
;;;      notice, this list of conditions and the following disclaimer.
;;;
;;;   2. Redistributions in binary form must reproduce the above copyright
;;;      notice, this list of conditions and the following disclaimer in the
;;;      documentation and/or other materials provided with the distribution.
;;;
;;;   3. Neither the name of the authors nor the names of its contributors
;;;      may be used to endorse or promote products derived from this
;;;      software without specific prior written permission.
;;;
;;;   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
;;;   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
;;;   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
;;;   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
;;;   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
;;;   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
;;;   TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
;;;   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
;;;   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
;;;   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
;;;   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
;;;

(define-module dbd.oracle
  (use dbi)
  (use gauche.sequence)
  (use util.relation)
  (use util.match)
  (use util.list)
  (use srfi-13)
  (use srfi-43)
  (export <oracle-driver> <oracle-connection> <oracle-query> <oracle-result>
          ))

(select-module dbd.oracle)

;; Loads extension
(dynamic-load "dbd_oracle")

(define-class <oracle-driver> (<dbi-driver>)
  ())

(define-class <oracle-connection> (<dbi-connection>)
  ((con :init-keyword :con)
   (err :init-keyword :err)))

(define-class <oracle-query> (<dbi-query>)
  ())

(define-class <oracle-result> (<relation> <sequence>)
  ((columns :init-keyword :columns :init-value '#())
   (rows    :init-keyword :rows :init-value '())))


(define-condition-type <dbd-oracle-error> <dbi-error> #f
  (error-code))

(define (%chkerr func . args)
  (let1 result (apply func args)
        (if (= (car result) 0)
            (cdr result)
            (let ((err (error->message (car result) (cdr result))))
              (error <dbd-oracle-error>
                     :error-code (car err)
                     (cdr err))))))

(define-method dbi-make-connection ((d <oracle-driver>)
                                    (options <string>)
                                    (option-alist <list>)
                                    . args)
  (let ([db (match option-alist
                   [((maybe-db . #t) . rest) maybe-db]
                   [else (assoc-ref option-alist "db" "")])]
        [user (get-keyword :username args #f)]
        [passwd (get-keyword :password args #f)]
        [err (%chkerr make-oracle-error)]
        )
    (make <oracle-connection>
      :con (%chkerr oracle-connect err user passwd db)
      :err err)))

;; replace place holders to :1, :2, ...
(define-method %replace-parameters ((sql <string>))
  (let ((n 0))
    (regexp-replace-all #/(--.*?\n|\/[*].*?[*]\/|\"[^\"]*\"|'[^']*'|\?)/
                        sql
                        (lambda (m)
                          (let ((matched (rxmatch-substring m)))
                            (if (string=? matched "?")
                                (string-append ":" (x->string (inc! n)))
                                matched))))))

(define-method dbi-prepare ((c <oracle-connection>)
                            (sql <string>)
                            . args)
  (let ((err (slot-ref c 'err))
        (replaced-sql (%replace-parameters sql)))
    (make <oracle-query> :connection c
          :prepared (%chkerr oracle-stmt-prepare err replaced-sql))))

(define-method dbi-execute-using-connection ((c <oracle-connection>)
                                             (q <oracle-query>)
                                             (params <list>))
  (let* ((con (slot-ref c 'con))
         (err (slot-ref c 'err))
         (stmt (slot-ref q 'prepared))
         (req (%chkerr oracle-stmt-bind-count err stmt))
         (len (length params)))
    (unless (= req len)
            (errorf <dbi-parameter-error>
                    "wrong-number of arguments: query requires ~d, but got ~d"
                    req len))
    (%oracle-stmt-bind-params! err stmt params)
    (%chkerr oracle-stmt-execute err con stmt)
    (if (= (%chkerr oracle-stmt-type err stmt) OCI_STMT_SELECT)
        (%make-oracle-result err stmt)
        (%chkerr oracle-stmt-row-count err stmt))))

(define-method %oracle-stmt-bind-params! ((err <oracle-error>)
                                          (stmt <oracle-stmt>)
                                          (params <list>))
  (let loop ([params params]
             [idx 0])
    (if (null? params)
        (undefined)
        (let1 val (car params)
              (cond
               [(integer? val)
                (oracle-stmt-bind-init err stmt idx BIND_INTEGER 0)
                (oracle-stmt-bind-set! err stmt idx val)]
               [(real? val)
                (oracle-stmt-bind-init err stmt idx BIND_REAL 0)
                (oracle-stmt-bind-set! err stmt idx val)]
               [else (let1 str (x->string val)
                           (oracle-stmt-bind-init err stmt idx BIND_STRING (string-size str))
                           (oracle-stmt-bind-set! err stmt idx str))])
              (loop (cdr params) (+ idx 1))))))

(define (%make-oracle-result err stmt)
  (let* ([params (%chkerr oracle-stmt-params err stmt)]
         [count (vector-length params)]
         [columns (make-vector count)])
    (let define-loop ([idx 0])
      (when (< idx count)
            (let* ((param (vector-ref params idx))
                   (data-type (slot-ref param 'data-type)))
              (vector-set! columns idx (string-downcase (slot-ref param 'name)))
              (cond ((= data-type SQLT_NUM)
                     (if (and (not (zero? (slot-ref param 'precision)))
                              (zero? (slot-ref param 'scale)))
                         (%chkerr oracle-stmt-column-init err stmt idx BIND_INTEGER 0)
                         (%chkerr oracle-stmt-column-init err stmt idx BIND_REAL 0)))
                    (else (%chkerr oracle-stmt-column-init err stmt idx BIND_STRING 4000)))
              (define-loop (+ idx 1)))))
    (let rows-loop ([rest? (%chkerr oracle-stmt-fetch err stmt)]
                    [rows '()])
      (if (not rest?)
          (make <oracle-result>
            :columns columns
            :rows (reverse! rows))
          (let1 row (make-vector count)
                (let row-loop ([idx 0])
                  (when (< idx count)
                        (vector-set! row idx (%chkerr oracle-stmt-column-ref err stmt idx))
                        (row-loop (+ idx 1))))
                (rows-loop (%chkerr oracle-stmt-fetch err stmt)
                           (cons row rows)))))))

(define-method dbi-open? ((c <oracle-connection>))
  (let1 con (slot-ref c 'con)
        (if con #t #f)))

(define-method dbi-open? ((q <oracle-query>))
  (let1 stmt (slot-ref q 'prepared)
        (if stmt #t #f)))

(define-method dbi-open? ((r <oracle-result>))
  (zero? (vector-length (slot-ref r 'columns))))

(define-method dbi-close ((c <oracle-connection>))
  (let ((con (slot-ref c 'con))
        (err (slot-ref c 'err)))
    (slot-set! c 'con #f)
    (slot-set! c 'err #f)
    (guard (e (else (oracle-error-close err) (raise e)))
           (%chkerr oracle-disconnect err con)
           (oracle-error-close err))))

(define-method dbi-close ((q <oracle-query>))
  (let1 stmt (slot-ref q 'prepared)
        (slot-set! q 'prepared #f)
        (oracle-stmt-close stmt)))

(define-method dbi-close ((r <oracle-result>))
  (unless (zero? (vector-length (slot-ref r 'columns)))
          (slot-set! r 'columns '#())
          (slot-set! r 'rows '()))
  (undefined))

(define-method call-with-iterator ((r <oracle-result>) proc . keys)
  (apply call-with-iterator (slot-ref r 'rows) proc keys))

(define-method relation-column-names ((r <oracle-result>))
  (slot-ref r 'columns))

(define-method relation-accessor ((r <oracle-result>))
  (let1 columns (slot-ref r 'columns)
        (lambda (row column . maybe-default)
          (cond
           [(vector-index (cut string=? <> column) columns)
            => (cut vector-ref row <>)]
           [(pair? maybe-default) (car maybe-default)]
           [else (error "oracle-result: invalid column:" column)]))))

(define-method relation-rows ((r <oracle-result>))
  (slot-ref r 'rows))

(define-method relation-modifier ((r <oracle-result>))
  #f)

;; Epilogue
(provide "dbd/oracle")
